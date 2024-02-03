#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

#include "PrimeEngine/Lua/LuaEnvironment.h"

#include "SoldierNPCMovementSM.h"
#include "SoldierNPCAnimationSM.h"
#include "SoldierNPC.h"
using namespace PE::Components;
using namespace PE::Events;
using namespace CharacterControl::Events;

namespace CharacterControl
{
    // Events sent by behavior state machine (or other high level state machines)
    // these are events that specify where a soldier should move
    namespace Events
    {
        PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_MOVE_TO, Event);

        SoldierNPCMovementSM_Event_MOVE_TO::SoldierNPCMovementSM_Event_MOVE_TO(Vector3 targetPos /* = Vector3 */)
            : m_targetPosition(targetPos),
              m_running(false)
        {
        }

        PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_STOP, Event);

        PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_TARGET_REACHED, Event);
        PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_SHOOT, Event);

        
    }

    namespace Components
    {
        PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM, Component);


        SoldierNPCMovementSM::SoldierNPCMovementSM(PE::GameContext& context, PE::MemoryArena arena, PE::Handle hMyself)
            : Component(context, arena, hMyself)
              , m_state(STANDING)
        {
        }

        SceneNode* SoldierNPCMovementSM::getParentsSceneNode()
        {
            PE::Handle hParent = getFirstParentByType<Component>();
            if (hParent.isValid())
            {
                // see if parent has scene node component
                return hParent.getObject<Component>()->getFirstComponent<SceneNode>();
            }
            return nullptr;
        }

        void SoldierNPCMovementSM::addDefaultComponents()
        {
            Component::addDefaultComponents();

            PE_REGISTER_EVENT_HANDLER(SoldierNPCMovementSM_Event_MOVE_TO,
                                      SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_MOVE_TO);
            PE_REGISTER_EVENT_HANDLER(SoldierNPCMovementSM_Event_STOP,
                                      SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_STOP);
            PE_REGISTER_EVENT_HANDLER(SoldierNPCMovementSM_Event_SHOOT,
                                      SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_SHOOT);
            PE_REGISTER_EVENT_HANDLER(Event_UPDATE, SoldierNPCMovementSM::do_UPDATE);
        }

        void SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_MOVE_TO(Event* pEvt)
        {
            auto pRealEvt = static_cast<SoldierNPCMovementSM_Event_MOVE_TO*>(pEvt);

            // change state of this state machine
            if (pRealEvt->m_running)
            {
                m_state = RUNNING_TO_TARGET;
                m_targetPostion = pRealEvt->m_targetPosition;

                OutputDebugStringA(
                    "PE:PROGRESS:SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_MOVE_TO() Received event, running: ");
                OutputDebugStringA(pRealEvt->m_running ? "true\n" : "false\n");

                // make sure the animations are playing

                PE::Handle h("SoldierNPCAnimSM_Event_RUN", sizeof(SoldierNPCAnimSM_Event_RUN));
                auto pOutEvt = new(h) SoldierNPCAnimSM_Event_RUN();

                SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
                pSol->getFirstComponent<SceneNode>()->handleEvent(pOutEvt);

                // release memory now that event is processed
                h.release();
            }
            else
            {
                m_state = WALKING_TO_TARGET;
                m_targetPostion = pRealEvt->m_targetPosition;

                OutputDebugStringA(
                    "PE:PROGRESS:SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_MOVE_TO() Received event, running: ");
                OutputDebugStringA(pRealEvt->m_running ? "true\n" : "false\n");

                // make sure the animations are playing

                PE::Handle h("SoldierNPCAnimSM_Event_WALK", sizeof(SoldierNPCAnimSM_Event_WALK));
                auto pOutEvt = new(h) SoldierNPCAnimSM_Event_WALK();

                SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
                pSol->getFirstComponent<SceneNode>()->handleEvent(pOutEvt);

                // release memory now that event is processed
                h.release();
            }
        }

        void SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_STOP(Event* pEvt)
        {
            SoldierNPCAnimSM_Event_STOP Evt;

            SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
            pSol->getFirstComponent<SceneNode>()->handleEvent(&Evt);
        }

        void SoldierNPCMovementSM::do_UPDATE(Event* pEvt)
        {
            if (m_state == WALKING_TO_TARGET || m_state == RUNNING_TO_TARGET)
            {
                // see if parent has scene node component
                SceneNode* pSN = getParentsSceneNode();
                if (pSN)
                {
                    Vector3 curPos = pSN->m_base.getPos();
                    float dsqr = (m_targetPostion - curPos).lengthSqr();

                    bool reached = true;
                    if (dsqr > 0.01f)
                    {
                        // not at the spot yet
                        auto pRealEvt = static_cast<Event_UPDATE*>(pEvt);
                        float speed = m_state == WALKING_TO_TARGET ? 1.4f : 3.0f;
                        float allowedDisp = speed * pRealEvt->m_frameTime;

                        Vector3 dir = (m_targetPostion - curPos);
                        dir.normalize();
                        float dist = sqrt(dsqr);
                        if (dist > allowedDisp)
                        {
                            dist = allowedDisp; // can move up to allowedDisp
                            reached = false; // not reaching destination yet
                        }

                        // instantaneous turn
                        pSN->m_base.turnInDirection(dir, 3.1415f);
                        pSN->m_base.setPos(curPos + dir * dist);
                    }

                    if (reached)
                    {
                        m_state = STANDING;

                        // target has been reached. need to notify all same level state machines (components of parent)
                        {
                            PE::Handle h("SoldierNPCMovementSM_Event_TARGET_REACHED",
                                         sizeof(SoldierNPCMovementSM_Event_TARGET_REACHED));
                            auto pOutEvt = new(h) SoldierNPCMovementSM_Event_TARGET_REACHED();

                            PE::Handle hParent = getFirstParentByType<Component>();
                            if (hParent.isValid())
                            {
                                hParent.getObject<Component>()->handleEvent(pOutEvt);
                            }

                            // release memory now that event is processed
                            h.release();
                        }

                        if (m_state == STANDING)
                        {
                            // no one has modified our state based on TARGET_REACHED callback
                            // this means we are not going anywhere right now
                            // so can send event to animation state machine to stop
                            {
                                SoldierNPCAnimSM_Event_STOP evt;
                                PE::Handle targetSceneNode = pSN->getFirstComponentHandle<SceneNode>();
                                Vector3 targetPos = targetSceneNode.getObject<SceneNode>()->m_base.getPos();
                                Vector3 dir = (targetPos - curPos);
                                dir.normalize();
                                // instantaneous turn
                                pSN->m_base.turnInDirection(dir, 3.1415f);
                                SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
                                pSol->getFirstComponent<SceneNode>()->handleEvent(&evt);
                            }
                        }
                    }
                }
            }
        }

        void SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_SHOOT(Event* pEvt)
        {
            if (m_state == STANDING)
            {
                // see if parent has scene node component
                auto pRealEvt = static_cast<SoldierNPCMovementSM_Event_SHOOT*>(pEvt);

                SceneNode* pSN = getParentsSceneNode();

                SoldierNPC* target_NPC = pRealEvt->m_targetNPC.getObject<SoldierNPC>();
                SceneNode* targetSN = target_NPC->getFirstComponent<SceneNode>();
                if (pSN && targetSN)
                {
                    SoldierNPCAnimSM_Event_SHOOT evt;
                    Vector3 curPos=pSN->m_base.getPos();
                    Vector3 targetPos=targetSN->m_base.getPos();
                    Vector3 dir = (targetPos - curPos);
                    dir.normalize();
                    // instantaneous turn
                    pSN->m_base.turnInDirection(dir, 3.1415f);
                    SoldierNPC* pSol = getFirstParentByTypePtr<SoldierNPC>();
                    pSol->getFirstComponent<SceneNode>()->handleEvent(&evt);
                }
            }
        }
    }
}
