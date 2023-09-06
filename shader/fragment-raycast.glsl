#version 330 core

// 3D texture containing normalized volume data
uniform sampler3D volume;

// 2D textures containing the front/back face coordinates
uniform sampler2D frontfaces;
uniform sampler2D backfaces;

// 2D texture containing transfer functions (1 per row)
uniform sampler2D transfer;

// Index of current transfer function
uniform float transferindex = 0.0;

// Camera position in world space
uniform vec3 camerapos;

// Inverted model matrix
uniform mat4 invertedmodel;

// Ray direction
in vec2 samplepos;

// Final fragment color
layout(location = 0) out vec4 out_color;


// Fetch an interpolated density value and the corresponding gradient
// for one texture index at once. Stores the gradient in .xyz and the
// density in .w of the returned vector.
vec4 GradientDensity(vec3 position)
{
    vec4 value;
    float a, b;

	a = textureOffset(volume, position, ivec3(1,0,0), 0).r;
	b = textureOffset(volume, position, ivec3(-1,0,0), 0).r;
    value.x = a - b;
    value.w = a + b;

	a = textureOffset(volume, position, ivec3(0,1,0), 0).r;
	b = textureOffset(volume, position, ivec3(0,-1,0), 0).r;
    value.y = a - b;
    value.w += a + b;

	a = textureOffset(volume, position, ivec3(0,0,1), 0).r;
	b = textureOffset(volume, position, ivec3(0,0,-1), 0).r;
    value.z = a - b;
    value.w += a + b;

    value.w /= 6.0;

    vec4 mul = step(-1.0, value) - step(1.0, value);
    value *= mul.x * mul.y * mul.z * mul.w;

    return value;
}

vec3 Illuminate(vec3 position, vec3 raydir, vec3 normal)
{
    raydir = normalize(raydir);
    normal = normalize(normal);
    vec3 lightpos = camerapos.xyz;
    vec3 lightcolor = vec3(1.0);

    const float kambient = 0.3;
    const float kdiffuse = 0.6;
    const float kspecular = 0.5;
    const float shininess = 50.0;

    vec3 pos2light = lightpos - position;
    pos2light = normalize(pos2light);

    vec3 half = raydir + pos2light;
    half = normalize(half);

    vec3 ambient = kambient * vec3(1.0);
    vec3 diffuse = kdiffuse * max(0.0, dot(pos2light, normal)) * lightcolor;
    vec3 specular = kspecular * pow(max(0.0, dot(half, normal)), shininess) * lightcolor;

    return ambient + diffuse + specular;
}

float SilhouetteModulation(vec3 gradient, vec3 viewdir)
{
    viewdir = normalize(viewdir);
    float opacity = 0.9f + 15.0f * pow(1.0 - abs(dot(gradient, viewdir)), 0.25);
    return min(opacity, 1.0);
}

float GradientMagnitudeModulation(vec3 gradient)
{
    float magnitude = length(gradient);
    float opacity = 0.8 + 5.0 * magnitude;
    return min(opacity, 1.0);
}

void main()
{
	out_color = vec4(0.0);

	// Ray entry and exit points in world space
	vec3 world_pos = texture(frontfaces, samplepos).xyz;
	vec3 world_exit = texture(backfaces, samplepos).xyz;

	if (world_pos == world_exit)
		return;

	// Ray entry and exit points in model space
	vec3 model_pos = (invertedmodel * vec4(world_pos, 1.0)).xyz;
	vec3 model_exit = (invertedmodel * vec4(world_exit, 1.0)).xyz;

	// Ray direction * distance between intersections
	vec3 world_dir = world_exit - world_pos;
	vec3 model_dir = model_exit - model_pos;

    // Number of steps for this ray
    float nstep = length(model_dir) * 2.0;

	// Per-step ray progression
	vec3 world_step = world_dir / nstep;
	vec3 model_step = model_dir / nstep;
	
	// Convert model space ray position/step to normalized texture coordinates
    vec3 volumesize = vec3(textureSize(volume, 0));
    model_pos = (model_pos + vec3(0.5)) / volumesize;
    model_step /= volumesize;
    
	// Size of transfer function texture
    float maxy = float(textureSize(transfer, 0).y);
	
	for (int i = 0; i < nstep; i++)
	{
		// 1st step: Sample volume at the current ray position
        vec4 tmp = GradientDensity(model_pos);
		float density = tmp.w;
		vec3 gradient = tmp.xyz;
        
		// 2nd step: Find a matching transfer function entry for the sample density
		// (here, we use 2 transfer functions and interpolate between them)
		vec2 cordbottom = vec2(density, transferindex / maxy);
        vec2 cordtop = vec2(density, (transferindex + 1.0) / maxy);
        
		vec4 texbottom = texture(transfer, cordbottom);
        vec4 textop = texture(transfer, cordtop);
        
		vec4 texel = mix(texbottom, textop, fract(transferindex));
		
		// 3rd Step: Shading / Illumination
		vec3 viewdir = world_pos - camerapos;

        texel.rgb *= Illuminate(world_pos, viewdir, gradient);

        texel.a *= GradientMagnitudeModulation(gradient);
        texel.a *= SilhouetteModulation(gradient, viewdir);

        out_color += (1.0 - out_color.a) * texel * 0.5;

		if (out_color.a >= 0.9)
		{
			break;
		}

        world_pos += world_step;
        model_pos += model_step;
    }
}

