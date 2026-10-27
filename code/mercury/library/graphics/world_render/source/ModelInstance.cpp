
#include "stdafx.h"

#include <world_render/ModelInstance.h>
#include <world_render/WorldRender.h>
#include <world_render/RenderModel.h>
#include <world_render/RenderModelData.h>
#include <world_render/RenderModelResourceUsage.h>
#include <SceneGraph/renderingdata/ModelInstanceCullNode.h>
#include <SceneGraph/renderingdata/PortalCullNode.h>
#include <system/Memory.h>
#include <renderingdata/RenderingData.h>
#include <SceneGraph/SceneGraph.h>

namespace WorldRender
{

// [Blur2PC] Share the original implementation without changing Windows source membership.
#if !defined(PLATFORM_MACOS)
#include "ModelInterpolation.inl"
#endif

// C_ModelInstance

C_ModelInstance::C_ModelInstance( const C_RenderModelData* pRenderModelData, const S32 modelIndex, const SceneGraph::C_SceneGraph* pSceneGraph ) :
	SceneGraph::C_ModelInstance(pSceneGraph, &pRenderModelData->GetScene(), pRenderModelData->GetModel(modelIndex).GetRenderListNode())
{
	if(pRenderModelData)
	{
		m_pRenderModel = &pRenderModelData->GetModel(modelIndex);
	
		MEMORY_TRACKER_PUSH("worldrender.modelinstance.materiallist");
		// copy material output data structure from render model
		if(m_pRenderListNode)
		{
			m_MaterialIndex = m_pRenderModel->GetMaterialIndex();
			m_pMaterialListData->CopyData(m_pRenderModel->GetMaterialList());
			m_pMaterialListTypes = &m_pRenderModel->GetMaterialList();
			m_pMaterialListDmaData = m_pMaterialListData->GetDmaData();
		}
		MEMORY_TRACKER_POP("worldrender.modelinstance.materiallist");

		BuildDefaultDynamicMaterialOutput();

		m_pszFriendlyName = m_pRenderModel->GetFriendlyName();
	}
	else
	{
		m_pRenderModel = NULL;
	}
}

C_ModelInstance::~C_ModelInstance()
{
#if PLATFORM_PC || defined(PLATFORM_MACOS) // [Blur2PC] Native rendering uses the same interpolation records.
	Blur2PCInterpForget(this);	// [Blur2PC]
#endif
	return;
}

void C_ModelInstance::BuildDefaultDynamicMaterialOutput()
{
	Maths::HMat43 matrix;

	matrix.Translation(Maths::HVec3(0.0f, 0.0f, 0.0f));

	for(S32 i = 0; i < m_pRenderModel->GetNumElements(); ++i)
	{
		SetElementMatrix(i, matrix);
	}

	m_pRenderModel->SetDefaultValues(m_pMaterialListData);
}

void C_ModelInstance::SetWorldSphere(const Maths::C_Sphere& sphere)
{
	if(GetCullNode())
	{
		GetCullNode()->SetSphere(sphere);
		/*if(GetCullNode()->GetType() == SceneGraph::I_ModelInstanceCullNode::MODELINSTANCECULLNODE_TYPE_PORTAL)
		{
			SceneGraph::C_PortalCullNode* pCullNode = (SceneGraph::C_PortalCullNode*)GetCullNode();
			assert(pCullNode);	// Sanity check.

			// [Oscar] temporary workaround until we get an official interface for disabling portal culling on specific model instances...
			if (pCullNode->GetSpatialProbeCount() > 0)
				pCullNode->SetSpatialProbeAtIndex(0, sphere.Centre());
		}

		// Flag the movement of the node to the SG.
		FlagMovement();*/
	}
	else
	{
		// Stop spam & slowdown:
		static S32 count = 0;
		if (count++ < 10)
			LOGWARN(g_Log, "ModelInstance: SetWorldSphere called but object not in SceneGraph");
	}
}

void C_ModelInstance::SetWorldSpherePosition(const Maths::HVec3& position)
{
	if(GetCullNode())
	{
		GetCullNode()->SetSpherePosition(position);
		/*if(GetCullNode()->GetType() == SceneGraph::I_ModelInstanceCullNode::MODELINSTANCECULLNODE_TYPE_PORTAL)
		{
			SceneGraph::C_PortalCullNode* pCullNode = (SceneGraph::C_PortalCullNode*)GetCullNode();
			assert(pCullNode);	// Sanity check.
			pCullNode->SetSpatialProbeAtIndex(0, position);
		}

		// Flag the movement of the node to the SG.
		FlagMovement();*/
	}
	else
	{
		// Stop spam & slowdown:
		static S32 count = 0;
		if (count++ < 10)
			LOGWARN(g_Log, "ModelInstance: SetWorldSphere called but object not in SceneGraph");
	}
}

Maths::C_Sphere C_ModelInstance::GetWorldSphere() const
{
	if(m_pCullNode)
		return m_pCullNode->GetSphere();
	else
		return Maths::C_Sphere();
}

void C_ModelInstance::SetNearFarDistance(const F32 nearDistance, const F32 farDistance)
{
	if(m_pCullNode)
		m_pCullNode->SetNearFarDistance(nearDistance, farDistance);
	else
		LOGWARN(g_Log, "ModelInstance: SetNearFarDistance called but object not in SceneGraph");
}


void C_ModelInstance::SetElementMatrix( const S32 elementIndex, const Maths::HMat43 PCREF m )
{
#if PLATFORM_PC || defined(PLATFORM_MACOS) // [Blur2PC] Native rendering uses the same interpolation records.
	if (Blur2PCInterpEnabled() && m_pRenderModel && m_pMaterialListData)
	{
		// [Blur2PC] render interpolation, recorded per destination matrix (a write to all elements
		// records each element, so a later write to one element owns that element)
		const S32 first = elementIndex < 0 ? 0 : elementIndex;
		const S32 last = elementIndex < 0 ? m_pRenderModel->GetNumElements() - 1 : elementIndex;
		for (S32 j = first; j <= last && j < m_pRenderModel->GetNumElements(); ++j)
		{
			const MaterialParameter* element = m_pRenderModel->GetWorldMatrixParameter(j);
			if (element != NULL && element->index >= 0 && element->index < m_pMaterialListData->GetNumMaterials())
			{
				RenderingData::Material& material = m_pMaterialListData->GetMaterial(element->index);
				Blur2PCInterpRecordStore(this, (MathsStorage::MatrixRow44*)material.GetData(element->offset), m);
			}
		}
	}
#endif
	WriteElementMatrix(elementIndex, m);
}

#if PLATFORM_PC || defined(PLATFORM_MACOS) // [Blur2PC] Native rendering uses the same interpolation records.
// [Blur2PC] SetElementMatrix for a model that spins with a wheel (see Blur2PCInterpRecordWheelStore)
void C_ModelInstance::Blur2PCSetWheelElementMatrix( const S32 elementIndex, const Maths::HMat43 PCREF m, const Maths::HMat43 PCREF wheel, const F32 spinRate )
{
	if (Blur2PCInterpEnabled() && m_pRenderModel && m_pMaterialListData)
	{
		const S32 first = elementIndex < 0 ? 0 : elementIndex;
		const S32 last = elementIndex < 0 ? m_pRenderModel->GetNumElements() - 1 : elementIndex;
		for (S32 j = first; j <= last && j < m_pRenderModel->GetNumElements(); ++j)
		{
			const MaterialParameter* element = m_pRenderModel->GetWorldMatrixParameter(j);
			if (element != NULL && element->index >= 0 && element->index < m_pMaterialListData->GetNumMaterials())
			{
				RenderingData::Material& material = m_pMaterialListData->GetMaterial(element->index);
				Blur2PCInterpRecordWheelStore(this, (MathsStorage::MatrixRow44*)material.GetData(element->offset), m, wheel, spinRate);
			}
		}
	}
	WriteElementMatrix(elementIndex, m);
}
#endif

// [Blur2PC] the original SetElementMatrix body: stores the matrix into the material data
void C_ModelInstance::WriteElementMatrix( const S32 elementIndex, const Maths::HMat43 PCREF m )
{
	if(!m_pRenderModel)
		return;

	Maths::HMat44 mat44(m);

	if(elementIndex < 0)
	{
		for(S32 j = 0, count = m_pRenderModel->GetNumElements(); j < count; ++j)
		{
			const MaterialParameter* element = m_pRenderModel->GetWorldMatrixParameter(j);
			
			if (element != NULL)
			{
				if (element->index >= 0)
				{
					RenderingData::Material& material = m_pMaterialListData->GetMaterial(element->index);
					Maths::Store(mat44, (MathsStorage::MatrixRow44A16*)material.GetData(element->offset));
				}
			}			
		}
	}
	else
	{
		if(elementIndex < m_pRenderModel->GetNumElements())
		{
			const MaterialParameter* element = m_pRenderModel->GetWorldMatrixParameter(elementIndex);

			if (element != NULL)
			{
				if(element->index >= 0 && element->index < m_pMaterialListData->GetNumMaterials())
				{
					RenderingData::Material& material = m_pMaterialListData->GetMaterial(element->index);
					Maths::Store(mat44, (MathsStorage::MatrixRow44A16*)material.GetData(element->offset));
				}
			}
		}
	}
}

Maths::HMat43 C_ModelInstance::GetElementMatrix( const S32 elementIndex ) const
{
	Maths::HMat43 mat;
	mat.SetIdentity();
	return mat;
}

void C_ModelInstance::SetParameter(const MaterialParameter& parameter, const void* pData)
{
	if (parameter.index < 0 || parameter.index >= m_pMaterialListData->GetNumMaterials())
		return;
	RenderingData::Material& material = m_pMaterialListData->GetMaterial(parameter.index);
	material.SetData(parameter.offset, pData, parameter.size);
}

U8* C_ModelInstance::GetParameter(const MaterialParameter& parameter)
{
	if (parameter.index < 0 || parameter.index >= m_pMaterialListData->GetNumMaterials())
		return 0;

	RenderingData::Material& material = m_pMaterialListData->GetMaterial(parameter.index);
	return material.GetData(parameter.offset);
}

const U8* C_ModelInstance::GetParameter(const MaterialParameter& parameter) const
{
	RenderingData::ConstMaterialReference material = m_pMaterialListData->RenderingData::MaterialListData::GetMaterial(parameter.index);
	return material.GetData(parameter.offset);
}

S32 C_ModelInstance::GetNumElements() const
{
	if(m_pRenderModel)
		return m_pRenderModel->GetNumElements();
	else
		return 0;
}	

void C_ModelInstance::AccumulateResourceUsage(RenderModelResourceUsage& ioResourceUsage) const
{
	RenderingData::Target::TextureInfoList renderingData;
	GetScene()->AccumulateTextureInfo(renderingData);
	ioResourceUsage.AccumulateRenderingDataTextures(renderingData);
}

} // namespace WorldRender
