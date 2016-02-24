//******************************************************************
//
// Copyright 2016 Samsung Electronics All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "SceneMemberResource.h"

#include <atomic>
#include "OCPlatform.h"

namespace OIC
{
    namespace Service
    {
        namespace
        {
            std::atomic_int numOfSceneMember(0);
        }

        SceneMemberResource::Ptr
        SceneMemberResource::createSceneMemberResource(
                RCSRemoteResourceObject::Ptr remoteObject)
        {
            SceneMemberResource::Ptr newSceneMemberObject(new SceneMemberResource());

            newSceneMemberObject->m_uri = PREFIX_SCENE_MEMBER_URI + "/" +
                std::to_string(numOfSceneMember++);

            newSceneMemberObject->m_remoteMemberObj = remoteObject;

            newSceneMemberObject->createResourceObject();
            newSceneMemberObject->initSetRequestHandler();
            newSceneMemberObject->setDefaultAttributes();

            return newSceneMemberObject;
        }

        SceneMemberResource::Ptr
        SceneMemberResource::createSceneMemberResource(const RCSResourceAttributes & link)
        {
            return createSceneMemberResource(RCSResourceAttributes(link));
        }

        SceneMemberResource::Ptr
        SceneMemberResource::createSceneMemberResource(RCSResourceAttributes && link)
        {
            auto href = link.at(SCENE_KEY_HREF).get<std::string>();

            std::string address;
            std::string uri;

            SceneUtils::getHostUriString(href, &address, &uri);

            auto ocResourcePtr
                = OC::OCPlatform::constructResourceObject(
                    address, uri, OCConnectivityType::CT_ADAPTER_IP, false,
                    link.at(SCENE_KEY_RT).get<std::vector<std::string>>(),
                    link.at(SCENE_KEY_IF).get<std::vector<std::string>>());

            return createSceneMemberResource(RCSRemoteResourceObject::fromOCResource(ocResourcePtr));
        }

        void SceneMemberResource::createResourceObject()
        {
            m_sceneMemberResourceObj
                = RCSResourceObject::Builder(
                        m_uri, SCENE_MEMBER_RT, OC_RSRVD_INTERFACE_DEFAULT).
                        setDiscoverable(true).setObservable(false).build();
        }

        void SceneMemberResource::setDefaultAttributes()
        {
            m_sceneMemberResourceObj->setAttribute(SCENE_KEY_ID, SceneUtils::OICGenerateUUIDStr());
            m_sceneMemberResourceObj->setAttribute(SCENE_KEY_NAME, std::string());

            RCSResourceAttributes subAtt;
            subAtt[SCENE_KEY_HREF]
                    = RCSResourceAttributes::Value(
                            m_remoteMemberObj->getAddress() + m_remoteMemberObj->getUri());
            subAtt[SCENE_KEY_IF] = RCSResourceAttributes::Value(m_remoteMemberObj->getInterfaces());
            subAtt[SCENE_KEY_RT] = RCSResourceAttributes::Value(m_remoteMemberObj->getTypes());
            m_sceneMemberResourceObj->setAttribute(SCENE_KEY_PAYLOAD_LINK, subAtt);

            m_sceneMemberResourceObj->setAttribute(
                    SCENE_KEY_SCENEMAPPINGS, std::vector<RCSResourceAttributes>());
            m_sceneMemberResourceObj->setAttribute(SCENE_KEY_URI, m_uri);
        }

        void SceneMemberResource::initSetRequestHandler()
        {
            m_requestHandler.m_owner = std::weak_ptr<SceneMemberResource>(shared_from_this());
            m_sceneMemberResourceObj->setSetRequestHandler(std::bind(
                    &SceneMemberResource::SceneMemberRequestHandler::onSetRequest,
                    m_requestHandler, std::placeholders::_1, std::placeholders::_2));
        }

        void SceneMemberResource::addMappingInfo(MappingInfo && mInfo)
        {
            RCSResourceAttributes newAtt;
            {
                RCSResourceObject::LockGuard guard(m_sceneMemberResourceObj);
                newAtt = m_sceneMemberResourceObj->getAttributes();
            }

            auto mappingInfo = newAtt.at(SCENE_KEY_SCENEMAPPINGS).
                    get<std::vector<RCSResourceAttributes>>();

            auto foundMInfo = std::find_if(mappingInfo.begin(), mappingInfo.end(),
                    [& mInfo](const RCSResourceAttributes & att) -> bool
                    {
                        return (att.at(SCENE_KEY_SCENE).get<std::string>() == mInfo.sceneName) &&
                                (att.at(SCENE_KEY_MEMBERPROPERTY).get<std::string>() == mInfo.key);
                    });

            if (foundMInfo != mappingInfo.end())
            {
                mappingInfo.erase(foundMInfo);
            }
            RCSResourceAttributes newMapInfo;
            newMapInfo[SCENE_KEY_SCENE] = RCSResourceAttributes::Value(mInfo.sceneName);
            newMapInfo[SCENE_KEY_MEMBERPROPERTY] = RCSResourceAttributes::Value(mInfo.key);
            newMapInfo[SCENE_KEY_MEMBERVALUE] = mInfo.value;
            mappingInfo.push_back(newMapInfo);

            m_sceneMemberResourceObj->setAttribute(SCENE_KEY_SCENEMAPPINGS, mappingInfo);
        }

        void SceneMemberResource::addMappingInfo(const MappingInfo & mInfo)
        {
            addMappingInfo(MappingInfo(mInfo));
        }

        std::vector<SceneMemberResource::MappingInfo> SceneMemberResource::getMappingInfo()
        {
            std::vector<MappingInfo> retMInfo;

            auto mInfo = m_sceneMemberResourceObj->getAttributeValue(SCENE_KEY_SCENEMAPPINGS).
                    get<std::vector<RCSResourceAttributes>>();
            std::for_each(mInfo.begin(), mInfo.end(),
                    [& retMInfo](const RCSResourceAttributes & att)
                    {
                        MappingInfo info(att.at(SCENE_KEY_SCENE).get<std::string>(),
                                att.at(SCENE_KEY_MEMBERPROPERTY).get<std::string>(),
                                att.at(SCENE_KEY_MEMBERVALUE));
                        retMInfo.push_back(info);
                    });

            return retMInfo;
        }

        std::string SceneMemberResource::getId() const
        {
            return m_sceneMemberResourceObj->getAttributeValue(SCENE_KEY_ID).get<std::string>();
        }

        std::string SceneMemberResource::getFullUri() const
        {
            return std::string(COAP_TAG + SceneUtils::getNetAddress() + m_uri);
        }

        RCSRemoteResourceObject::Ptr SceneMemberResource::getRemoteResourceObject() const
        {
            return m_remoteMemberObj;
        }

        RCSResourceObject::Ptr SceneMemberResource::getRCSResourceObject() const
        {
            return m_sceneMemberResourceObj;
        }

        void SceneMemberResource::execute(std::string && sceneName)
        {
            execute(std::move(sceneName), nullptr);
        }

        void SceneMemberResource::execute(const std::string & sceneName)
        {
            execute(std::string(sceneName));
        }

        void SceneMemberResource::execute(std::string && sceneName, MemberexecuteCallback executeCB)
        {
            RCSResourceAttributes setAtt;

            auto mInfo = getMappingInfo();
            std::for_each (mInfo.begin(), mInfo.end(),
                    [& setAtt, & sceneName](const MappingInfo & info)
                    {
                        if(info.sceneName == sceneName)
                        {
                            setAtt[info.key] = info.value;
                        }
                    });

            if (setAtt.empty() && executeCB != nullptr)
            {
                executeCB(RCSResourceAttributes(), SCENE_RESPONSE_SUCCESS);
            }

            m_remoteMemberObj->setRemoteAttributes(setAtt, executeCB);
        }

        void SceneMemberResource::execute(
                const std::string & sceneName, MemberexecuteCallback executeCB)
        {
            execute(std::string(sceneName), std::move(executeCB));
        }

        void SceneMemberResource::setName(const std::string & name)
        {
            setName(std::string(name));
        }

        void SceneMemberResource::setName(std::string && name)
        {
            m_sceneMemberResourceObj->setAttribute(SCENE_KEY_NAME, std::move(name));
        }

        std::string SceneMemberResource::getName() const
        {
            return m_sceneMemberResourceObj->getAttributeValue(SCENE_KEY_NAME).toString();
        }

        RCSSetResponse SceneMemberResource::SceneMemberRequestHandler::
        onSetRequest(const RCSRequest & request, RCSResourceAttributes & attributes)
        {
            if (attributes.contains(SCENE_KEY_SCENEMAPPINGS))
            {
                addMappingInfos(request, attributes);
            }

            if (attributes.contains(SCENE_KEY_NAME))
            {
                setSceneMemberName(request, attributes);
            }

            return RCSSetResponse::create(attributes, SCENE_CLIENT_BADREQUEST).
                    setAcceptanceMethod(RCSSetResponse::AcceptanceMethod::IGNORE);
        }

        RCSSetResponse
        SceneMemberResource::SceneMemberRequestHandler::addMappingInfos(
                const RCSRequest & /*request*/, RCSResourceAttributes & attributes)
        {
            auto ptr = m_owner.lock();
            if (!ptr)
            {
                return RCSSetResponse::create(attributes, SCENE_CLIENT_BADREQUEST).
                        setAcceptanceMethod(RCSSetResponse::AcceptanceMethod::IGNORE);
            }

            auto mInfo = attributes.at(SCENE_KEY_SCENEMAPPINGS).
                    get<std::vector<RCSResourceAttributes>>();
            std::for_each (mInfo.begin(), mInfo.end(),
                    [& ptr](const RCSResourceAttributes & att)
                    {
                        ptr->addMappingInfo(SceneMemberResource::MappingInfo(
                                att.at(SCENE_KEY_SCENE).get<std::string>(),
                                att.at(SCENE_KEY_MEMBERPROPERTY).get<std::string>(),
                                att.at(SCENE_KEY_MEMBERVALUE)));
                    });

            return RCSSetResponse::create(attributes).
                    setAcceptanceMethod(RCSSetResponse::AcceptanceMethod::IGNORE);
        }

        RCSSetResponse
        SceneMemberResource::SceneMemberRequestHandler::setSceneMemberName(
                const RCSRequest & /*request*/, RCSResourceAttributes & attributes)
        {
            auto ptr = m_owner.lock();
            if (!ptr)
            {
                return RCSSetResponse::create(attributes, SCENE_CLIENT_BADREQUEST).
                        setAcceptanceMethod(RCSSetResponse::AcceptanceMethod::IGNORE);
            }

            ptr->setName(attributes.at(SCENE_KEY_NAME).get<std::string>());

            return RCSSetResponse::create(attributes).
                    setAcceptanceMethod(RCSSetResponse::AcceptanceMethod::IGNORE);
        }
    }
}
