/*
 *
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/SCA_CollectionActuator.cpp
 *  \ingroup ketsji
 */


#include "SCA_IActuator.h"
#include "SCA_CollectionActuator.h"
#include <iostream>

#include "KX_GameObject.h"
#include "KX_Scene.h"

extern "C" {
  #include "BKE_collection.h"
}

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_CollectionActuator::SCA_CollectionActuator(SCA_IObject *gameobj,
                                               KX_Scene *scene,
                                               KX_Camera *cam,
                                               Collection *collection,
                                               int mode,
                                               bool use_logic,
                                               bool use_physics,
                                               bool use_visibility)
    : SCA_IActuator(gameobj, KX_ACT_COLLECTION),
    m_kxscene(scene),
	m_collection(collection),
    m_camera(cam),
	m_mode(mode),
    m_useLogic(use_logic),
    m_usePhysics(use_physics),
    m_useVisibility(use_visibility)
{
} /* End of constructor */



SCA_CollectionActuator::~SCA_CollectionActuator()
{
} /* end of destructor */



CValue* SCA_CollectionActuator::GetReplica()
{
  SCA_CollectionActuator* replica = new SCA_CollectionActuator(*this);
  replica->ProcessReplica();
  return replica;
}

void SCA_CollectionActuator::ProcessReplica()
{
  SCA_IActuator::ProcessReplica();
}

bool SCA_CollectionActuator::UnlinkObject(SCA_IObject* clientobj)
{
  /*if (clientobj == (SCA_IObject*)m_camera)
  {
    return true;
  }*/
  return false;
}

void SCA_CollectionActuator::Relink(std::map<SCA_IObject *, SCA_IObject *>& obj_map)
{
}


bool SCA_CollectionActuator::Update()
{
  // bool result = false;	/*unused*/
  bool bNegativeEvent = IsNegativeEvent();
  RemoveAllEvents();

  if (bNegativeEvent)
    return false; // do nothing on negative events
	
  switch (m_mode) {
    case KX_COLLECTION_SUSPEND:
      for (KX_GameObject *gameobj : m_kxscene->GetObjectList()) {
        Object *ob = gameobj->GetBlenderObject();
        if (ob && BKE_collection_has_object(m_collection, ob)) {
          if (m_useLogic) {
            gameobj->SuspendLogic(true);
          }
          if (m_usePhysics) {
            gameobj->SuspendPhysics(false, true);
          }
          if (m_useVisibility) {
            gameobj->SetVisible(false, true);
          }
        }
      }
      break;
    case KX_COLLECTION_RESUME:
      for (KX_GameObject *gameobj : m_kxscene->GetObjectList()) {
        Object *ob = gameobj->GetBlenderObject();
        if (ob && BKE_collection_has_object(m_collection, ob)) {
          if (m_useLogic) {
            gameobj->RestoreLogic(true);
          }
          if (m_usePhysics) {
            gameobj->RestorePhysics(true);
          }
          if (m_useVisibility) {
            gameobj->SetVisible(true, true);
          }
        }
      }
      break;
    case KX_COLLECTION_ADD_OVERLAY:
      if (m_camera && m_kxscene->CameraIsInactive(m_camera)) {
        m_kxscene->AddOverlayCollection(m_camera, m_collection);
      }
      else {
        std::cout << "Collection Actuator: Inexistant Camera or Camera already used by main "
                     "render or ImageRender"
                  << std::endl;
      }
      break;
    case KX_COLLECTION_REMOVE_OVERLAY:
      m_kxscene->RemoveOverlayCollection(m_collection);
      break;
    default:
      break;
  }

  return false;
}


#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_CollectionActuator::Type = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"SCA_CollectionActuator",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_CollectionActuator::Methods[] =
{
	{nullptr,nullptr} //Sentinel
};

PyAttributeDef SCA_CollectionActuator::Attributes[] = {
	//KX_PYATTRIBUTE_STRING_RW("scene",0,MAX_ID_NAME-2,true,SCA_SceneActuator,m_nextSceneName),
	//KX_PYATTRIBUTE_RW_FUNCTION("camera",SCA_SceneActuator,pyattr_get_camera,pyattr_set_camera),
	//KX_PYATTRIBUTE_BOOL_RW("useRestart", SCA_SceneActuator, m_restart),
	//KX_PYATTRIBUTE_INT_RW("mode", KX_SCENE_NODEF+1, KX_SCENE_MAX-1, true, SCA_SceneActuator, m_mode),
	KX_PYATTRIBUTE_NULL	//Sentinel
};

#endif // WITH_PYTHON

/* eof */
