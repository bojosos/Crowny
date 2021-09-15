#type vertex

attribute highp vec4 a_Position;
attribute highp vec2 a_UV;
attribute highp float a_Tid;
attribute highp vec4 a_Color;

uniform highp mat4 u_ProjectionMatrix;
uniform highp mat4 u_ViewMatrix;
uniform highp mat4 u_ModelMatrix;

varying highp vec4 vs_Position;
varying highp vec2 vs_Uv;
varying highp float vs_Tid;
varying highp vec4 vs_Color;

void main()
{
	gl_Position = u_ProjectionMatrix * u_ViewMatrix * u_ModelMatrix * a_Position;

	vs_Position = u_ModelMatrix * a_Position;
	vs_Uv = a_UV;
	vs_Tid = a_Tid;
	vs_Color = a_Color;
}

#type fragment

varying highp vec4 vs_Position;
varying highp vec2 vs_Uv;
varying highp float vs_Tid;
varying highp vec4 vs_Color;

//uniform sampler2D u_Textures[32];
uniform sampler2D u_Texture;

void main(void) {

	highp vec4 texColor = vs_Color;
	if (vs_Tid > 0.0)
	{
		int tid = int(vs_Tid - 0.5);
		//texColor = vs_in.color * texture(u_Textures[tid], vs_in.uv);
		texColor = vs_Color * texture2D(u_Texture, vs_Uv);
		//texColor = vec4(1.0, 1.0, 1.0, texture(u_Texture, vs_in.uv).r);
	}

	gl_FragColor = texColor;
}

