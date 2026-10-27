#ifndef UI_LIGHTPAINTSYSTEM_H
#define UI_LIGHTPAINTSYSTEM_H

namespace DynamicEffect { namespace Particle { namespace XtInterface { struct Event; } } }

struct SparkleControl
{
	Maths::StorageVector3 position;
};

void LightPaint_Init(int objectCount, int trailCount);
void LightPaint_Destroy();
#if defined(PLATFORM_MACOS)
// [Blur2PC] Required GPU acquisition status for the owning UI layer.
bool LightPaint_IsCreated();
#endif

void LightPaint_Emit(XtU32 colour, UI::C_TextureHandleIndex texture, const XtHandle<DynamicEffect::Particle::XtInterface::Event>& particleEvent, int layer, int panel, float width, int controlCount, SparkleControl* controls);
void LightPaint_ClearAll();

void LightPaint_Update(float tick);
void LightPaint_Render();

#endif // UI_LIGHTPAINTSYSTEM_H
