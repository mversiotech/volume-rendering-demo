#version 330 core

// Vertex coordinates of on-screen cube
layout(location = 0) in vec2 vertex;

// Texture coordinates
layout(location = 1) in vec2 uv;

// Sampling position for front/back face buffer
out vec2 samplepos;

void main()
{
	gl_Position = vec4(vertex, 0.0, 1.0);
	samplepos = uv;
}
