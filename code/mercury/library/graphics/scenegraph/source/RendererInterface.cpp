#include "stdafx.h"

#include "SceneGraph/SceneGraphImpl.h"
#include "SceneGraph/RendererInterface.h"
#include "SceneGraph/View.h"

namespace SceneGraph
{

I_Renderer::I_Renderer(C_SceneGraph* pSceneGraph) : m_pSceneGraph(pSceneGraph)
{
	if(m_pSceneGraph)
		m_pSceneGraph->AddRenderer(this);
}
	

I_Renderer::~I_Renderer()
{
	if(m_pSceneGraph)
		m_pSceneGraph->RemoveRenderer(this);
}

#ifdef PLATFORM_MACOS
// [Blur2PC] Keep the template independent of the full scene-view definition.
S32 I_Renderer::NativeViewIndex(const C_View* pParent) const
{
	return pParent->GetIndex();
}
#endif

U32 I_Renderer::ViewEnabled(const C_View* pView)
{
	return pView->RenderNodeEnabled(m_NodeFlag);
}

} // namespace SceneGraph

