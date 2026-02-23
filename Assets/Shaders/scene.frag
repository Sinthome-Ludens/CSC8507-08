#version 400 core

uniform vec4 		objectColour;
uniform sampler2D 	mainTex;
uniform sampler2DShadow shadowTex;

uniform vec3	sunPos;
uniform float	sunRadius;
uniform vec3	sunColour;

uniform vec3	cameraPos;

uniform bool hasTexture;

in Vertex
{
	vec4 colour;
	vec2 texCoord;
	vec4 shadowProj;
	vec3 normal;
	vec3 worldPos;
} IN;

out vec4 fragColor;

void main(void)
{
	float shadow = 1.0; // New !
	
	if( IN . shadowProj . w > 0.0) { // New !
		shadow = textureProj ( shadowTex , IN . shadowProj ) * 0.5f;
	}

	vec3  incident = normalize ( sunPos - IN.worldPos );
	float lambert  = max (0.0 , dot ( incident , IN.normal )) * 0.9; 
	
	vec3 viewDir = normalize ( cameraPos - IN . worldPos );
	vec3 halfDir = normalize ( incident + viewDir );

	float rFactor = max (0.0 , dot ( halfDir , IN.normal ));
	float sFactor = pow ( rFactor , 80.0 );
	
	vec4 albedo = IN.colour;

	if(hasTexture) {
		albedo *= texture(mainTex, IN.texCoord);
		// 手动 sRGB→Linear：NCL 的 OGLTexture 使用 GL_RGBA 内部格式（非 sRGB），
		// 需要在 shader 中做 gamma 展开，确保光照计算在线性空间进行
		albedo.rgb = pow(albedo.rgb, vec3(2.2));
	}
	
	fragColor.rgb = albedo.rgb * 0.05f; //ambient
	
	fragColor.rgb += albedo.rgb * sunColour.rgb * lambert * shadow; //diffuse light
	
	fragColor.rgb += sunColour.rgb * sFactor * shadow; //specular light

	// HDR 线性输出，由后处理统一做 tone mapping + gamma

	fragColor.a = albedo.a;
}