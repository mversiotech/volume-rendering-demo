#version 330 core

// Vertex coordinates of bounding box
layout(location = 0) in vec3 vertex;

// View Matrix
uniform mat4 view;

// Projection matrix
uniform mat4 projection;

// Per-fragment position
out vec4 fragpos;

void main()
{
	fragpos = vec4(vertex, 1.0);
	gl_Position = projection * view * fragpos;
}
