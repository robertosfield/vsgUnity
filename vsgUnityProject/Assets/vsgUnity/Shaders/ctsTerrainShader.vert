#version 450
#pragma import_defines ( VSG_NORMAL, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING )
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
    //mat3 normal;
} pc;

layout(location = 0) in vec3 vsg_Vertex;

#ifdef VSG_NORMAL
layout(location = 1) in vec3 vsg_Normal;
layout(location = 1) out vec3 normalDir;
#endif

#ifdef VSG_TEXCOORD0
layout(location = 4) in vec2 vsg_MultiTexCoord0;
layout(location = 4) out vec2 texCoord0;
#endif

#ifdef VSG_LIGHTING
layout(location = 5) out vec3 viewDir;
layout(location = 6) out vec3 lightDir;
#endif

out gl_PerVertex{ vec4 gl_Position; };

void main()
{
    gl_Position = (pc.projection * pc.modelview) * vec4(vsg_Vertex, 1.0);
#ifdef VSG_TEXCOORD0
    texCoord0 = vsg_MultiTexCoord0.st;
#endif
#ifdef VSG_NORMAL
    vec3 n = ((pc.modelview) * vec4(vsg_Normal, 0.0)).xyz;
    normalDir = n;
#endif
#ifdef VSG_LIGHTING
    vec4 lpos = /*vsg_LightSource.position*/ vec4(0.0, 0.25, 1.0, 0.0);
    viewDir = -vec3((pc.modelview) * vec4(vsg_Vertex, 1.0));
    if (lpos.w == 0.0)
        lightDir = lpos.xyz;
    else
        lightDir = lpos.xyz + viewDir;
#endif
}
