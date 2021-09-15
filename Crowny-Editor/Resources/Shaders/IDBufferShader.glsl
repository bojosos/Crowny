#type vertex
#version 430 core

layout(location = 0) in vec4 a_Position;
layout(location = 1) in int a_ObjectID;

uniform mat4 u_ProjectionMatrix;
uniform mat4 u_ViewMatrix;

out flat int ObjectID;

void main() {
    ObjectID = a_ObjectID;
	gl_Position = u_ProjectionMatrix * u_ViewMatrix * a_Position;
}

#type fragment
#version 430 core
layout(location = 0) out int o_ObjectID;

in flat int ObjectID;

void main() {
    o_ObjectID = ObjectID;
}