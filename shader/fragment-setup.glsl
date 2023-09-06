#version 330 core

// Fragment position
in vec4 fragpos;

// Ray/bounds intersection
layout(location = 0) out vec4 position;

void main()
{
	position = vec4(fragpos.rgb, 1.0);
}
