#!/usr/bin/env python3
"""[Blur2PC] Copy pinned MojoShader Metal profile with actual TEXLDL semantics.

Upstream currently aliases TEXLDL to implicit TEXLD. Retain its decoder,
register formatting, projections and all other instruction emitters unchanged.
"""
from pathlib import Path
import argparse

def patch(text):
    def once(before, after):
        nonlocal text
        if text.count(before) != 1:
            raise RuntimeError('Pinned MojoShader TEXLDL patch context changed: '+before[:65])
        text=text.replace(before,after,1)
    once('static void metal_texld(Context *ctx, const int texldd)',
         'static void metal_texld(Context *ctx, const int texldd, const int texldl)')
    once('    metal_texld(ctx, 0);','    metal_texld(ctx, 0, 0);')
    once('    metal_texld(ctx, 1);','    metal_texld(ctx, 1, 0);')
    once('''        // Metal doesn't have a texture2DProj() function''','''        // [Blur2PC] TEXLDL consumes source.w explicitly in both stages.
        if (texldl)
        {
            biasleft = ", level(";
            make_METAL_srcarg_string_w(ctx, 0, bias, sizeof (bias));
            biasright = ")";
        }

        // Metal doesn't have a texture2DProj() function''')
    once('''    // !!! FIXME: The spec says we can't use GLSL's texture*Lod() built-ins
    // !!! FIXME:  from fragment shaders for some inexplicable reason.
    // !!! FIXME:  Maybe Metal can do it, but I haven't looked into it yet.
    emit_METAL_TEXLD(ctx);''','''    // [Blur2PC] Keep the original explicit operand for the shared PC LOD ABI.
    metal_texld(ctx, 0, 1);''')
    return text

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source',type=Path);ap.add_argument('output',type=Path)
    args=ap.parse_args();result=patch(args.source.read_text())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    if not args.output.exists() or args.output.read_text()!=result:args.output.write_text(result)
