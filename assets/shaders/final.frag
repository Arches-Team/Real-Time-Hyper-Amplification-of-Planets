#version 450 core

layout (location = 0) in vec3 proxyPos;

layout (location = 0) out vec4 outColor;

uniform dmat4 inverseViewProj;
uniform dvec3 cameraPosition;
uniform dvec2 screenSizePixels;
uniform double renderScale;
uniform double planetRadiusKm;

uniform vec3 sunDirection;
uniform float seaLevelKm; 
uniform float waterArtificialOpacity = 1.0;
uniform float atmosphereDepthKm;

const int deactivateSnowFx = 0;

uniform sampler2D terrainGeometryBuffer0; // xyz: world pos - w: altitude above sea level
uniform sampler2D terrainGeometryBuffer1; // xyz: world normal - w: distance2Cam
uniform sampler2D terrainGeometryBuffer2; // x: biome (1=desert), yzw: unused
uniform sampler2D waterGeometryBuffer0; // xyz: world pos - w: altitude above sea level
uniform sampler2D waterGeometryBuffer1; // xyz: world normal - w: distance2Cam

uniform sampler2D grassTexture;
uniform sampler2D rockTexture;
uniform sampler2D desertTexture;
uniform sampler2D snowTexture;
/*uniform sampler2DArray terrainTexture;//0: grass, 1: rock, 2: desert, 3: snow
#define TEXTURE_LAYER_GRASS			0
#define TEXTURE_LAYER_ROCK			1
#define TEXTURE_LAYER_DESERT		2
#define TEXTURE_LAYER_SNOW			3
*/
//uniform sampler2D oceanFloorTexture;

uniform sampler3D noiseTexture;

const vec3 cameraPositionf = vec3(cameraPosition);
const float worldScale = float(renderScale);
const float planetRadius = float(planetRadiusKm / renderScale);
const float seaLevel = seaLevelKm / worldScale;
const float atmosphereRadius = planetRadius + seaLevel + (atmosphereDepthKm / worldScale);
const vec3 campos_normed = normalize(cameraPositionf);
const float camera_altitude = float(length(cameraPosition)) - (planetRadius + seaLevel);



///////////////////////////////////// CONST PARAMS /////////////////////////////////////
const vec3 SUN_DIR = normalize(sunDirection);
const float ATMOS_CORRECTION = mix(0.5 + 0.5 * (1.0 - max(0.0, dot(SUN_DIR, campos_normed))), 1.0, smoothstep(50.0, 250.0, camera_altitude*worldScale));//1.0 means no cheating
const vec3 SUN_COLOR = vec3(1.0, 1.0, 0.98);
const vec3 SUN_LIGHT = SUN_COLOR * 22.0;//mix(16.0, 1.0, (ATMOS_CORRECTION - 0.5)/1.1);

const float RAYLEIGH_SCALEHEIGHT = 8.4/worldScale; // 8.4 km
const float MIE_SCALEHEIGHT = 1.2/worldScale;
const vec3 RAYLEIGH_COEFF = vec3(0.0058, 0.0135, 0.0331)*worldScale; // Rayleigh Scattering Coefficients (wavelength dependent)
const float MIE_COEFF = 0.0021 * worldScale;//0.0021*worldScale;

//const vec3 SEAWATER_EXTINCTION_RGB = vec3(3.8, 1.1, 0.6) * worldScale; // "real" values
const vec3 SEAWATER_EXTINCTION_RGB = vec3(3.8, 1.4, 0.9) * worldScale; // Values adapted to balance the contrast step (see main)
const vec3 RIVER_EXTINCTION_RGB = 128.0 * vec3(3.8, 0.75, 1.0) * worldScale;
vec3 WATER_EXTINCTION_RGB;

// - other -
const float LN2 = 0.69314781; // natural logarithm of 2.0
const float INVLN2 = 1.0 / LN2;
////////////////////////////////////////////////////////////////////////////////////////




const dvec2 _NO_INTERSECTION = dvec2(0.0LF);
dvec2 _rayIntersectSphere(double sphereRadius, dvec3 sphereCenter, dvec3 rayOrigin, dvec3 ray)
{
  // renvoie les deux potentielles intersections avec la sphère.
  
  double t1, t2;
  dvec3 cam = rayOrigin - sphereCenter;
  double A = dot(ray, ray);
  double C = dot(cam, cam) - sphereRadius*sphereRadius;
  dvec3 CRP = 2.0LF * cam * ray;
  double B = CRP.x + CRP.y + CRP.z;
  double delta = B*B - 4.0LF*A*C;
  if (delta < 0.0LF)
    return _NO_INTERSECTION;
  double iA = 1.0LF/(2.0LF*A);
  double sq = sqrt(delta);
  t1 = (-B - sq) * iA;
  t2 = (-B + sq) * iA;
  return dvec2(t1, t2);
}

const vec2 NO_INTERSECTION = vec2(0.0);
vec2 rayIntersectSphere(float sphereRadius, vec3 sphereCenter, vec3 rayOrigin, vec3 ray)
{
  // renvoie les deux potentielles intersections avec la sphère.
  
  float t1, t2;
  vec3 cam = rayOrigin - sphereCenter;
  float A = dot(ray, ray);
  float C = dot(cam, cam) - sphereRadius*sphereRadius;
  vec3 CRP = 2.0 * cam * ray;
  float B = CRP.x + CRP.y + CRP.z;
  float delta = B*B - 4.0*A*C;
  if (delta < 0.0)
    return NO_INTERSECTION;
  float iA = 1.0/(2.0*A);
  float sq = sqrt(delta);
  t1 = (-B - sq) * iA;
  t2 = (-B + sq) * iA;
  return vec2(t1, t2);
}

float airdensity(vec3 start, vec3 end, float scaleheight, float numsamples)
{
  vec3 ray = end - start;
  const float sampleStep = length(ray) / numsamples;  
  ray = normalize(ray);
    
  float density = 0.0;
  for (float i = 0.0; i < numsamples; i+=1.0)
  {
	vec3 samplePoint = start + (i + 0.5) * sampleStep * ray;
	float h = max(0.0, length(samplePoint) - planetRadius - seaLevel*ATMOS_CORRECTION);
	density += sampleStep * exp2( -INVLN2 * h /scaleheight );
  }
  return density;
}

float airdensity2(vec3 p, float range, float scaleheight)
{  
  //const float sunsetFactor = 0.3 + 0.7 * (1.0 - max(0.0, dot(SUN_DIR, campos_normed))); // cheat!
  float h = max(0.0, length(p) - planetRadius - seaLevel*ATMOS_CORRECTION);
  float e = exp2( -INVLN2 * h /scaleheight );
  return range * e;
}

float getCloudDensity(vec3 point, float altitude, float base, float top, float depth)
{	
	float height_falloff = smoothstep(0.0, 0.16*depth, altitude - base) * smoothstep(0.0, 0.16*depth, top - altitude);	
	vec3 texcoords = vec3(point.x, point.y, point.z * 3.0);
	//return height_falloff * (0.7 * smoothstep(0.5, 0.62, texture(noiseTexture, texcoords + 4.0*sin(point.z)).g) + 0.3*texture(noiseTexture, texcoords*8.0).r) * /*smoothstep(0.4, 0.6, texture(noiseTexture, texcoords*0.15 + 17.7).g) **/ smoothstep(0.45, 0.7, texture(noiseTexture, texcoords*0.05 + 5.3).g);
	return height_falloff * (1.0 * smoothstep(0.382, 0.62, texture(noiseTexture, texcoords).g) * 1.0*texture(noiseTexture, texcoords*7.0).g) * smoothstep(0.5, 0.7, texture(noiseTexture, texcoords*0.03 + 5.3).g) * texture(noiseTexture, texcoords*0.2).r;
}

void atmosphereAndCloudsScattering(in float cloud_distance_start, in float cloud_distance_end, in vec3 entryPoint, in vec3 ray, in float sampleRange, in vec3 lightDir, in vec3 light
                          , out vec3 transmittance, out vec3 inscatter)
{
	const float ATMSAMPLES = 6.0;
	const float CLOUDSAMPLES = 3.0;
	
	float atmSamples_part1 = 0.0;
	float atmSamples_part2 = 0.0;
	if (cloud_distance_start > 0.0)
		atmSamples_part1 = ATMSAMPLES;
	if (cloud_distance_end < sampleRange)
		atmSamples_part2 = ATMSAMPLES;
	
	//const vec3 sampleEnd = sampleStart + sampleRange * ray;
	float sampleStep;

	const float h_clouds = 13.0 / worldScale;//14 km, cirrus
	const float cloud_depth = 0.3 / worldScale;//800m

	inscatter = vec3(0.0);
	vec3 extinctionRayleigh = vec3(0.0);
	vec3 extinctionMie = vec3(0.0);

	const float cosTheta = dot(ray, lightDir);
	const float c = 1.0 + cosTheta*cosTheta;
	const float rayleighPhase = 0.0596831 * c;
	const float miePhase = 0.050421 * c / (2.5776 * pow(1.5776 - 1.52*cosTheta, 1.5));// Cornette-Shanks phase function with g = 0.76

	// --- sample atmosphere (part1 before clouds) ---
	sampleStep = cloud_distance_start / atmSamples_part1;
	for (float i=0.0; i < atmSamples_part1; i+=1.0)
	{    
		const float t = (i+0.5)*sampleStep;    
		const vec3 samplePoint = entryPoint + t*ray;
		const float altitude = length(samplePoint);
			
		extinctionRayleigh += airdensity2(samplePoint, sampleStep, RAYLEIGH_SCALEHEIGHT);
		extinctionMie += airdensity2(samplePoint, sampleStep, MIE_SCALEHEIGHT);
		transmittance = exp2( - INVLN2 * (RAYLEIGH_COEFF * extinctionRayleigh + 1.1 * MIE_COEFF * extinctionMie));	

		vec2 exitIntersection = rayIntersectSphere(atmosphereRadius, vec3(0.0), samplePoint, lightDir);
		float exitDist = exitIntersection.y;  
		vec3 exitTransmittance = exp2(-INVLN2 *
			(
			  RAYLEIGH_COEFF  * airdensity(samplePoint, samplePoint + exitDist*lightDir, RAYLEIGH_SCALEHEIGHT, 3.0)
			  + 1.1*MIE_COEFF * airdensity(samplePoint, samplePoint + exitDist*lightDir, MIE_SCALEHEIGHT, 3.0)
			));

		float h = max(0.0, altitude - planetRadius - seaLevel * ATMOS_CORRECTION);
		vec3 rayleighScattering = RAYLEIGH_COEFF * exp2(-INVLN2 * h / RAYLEIGH_SCALEHEIGHT);
		vec3 mieScattering = vec3(MIE_COEFF * exp2(-INVLN2 * h / MIE_SCALEHEIGHT));


		vec3 sampleScatter = sampleStep * exitTransmittance * light * (rayleighPhase * rayleighScattering + miePhase * mieScattering);
		sampleScatter *= transmittance;	
		
		inscatter += sampleScatter;
	}
	
	// --- sample clouds and atmosphere ---
	sampleStep = (cloud_distance_end - cloud_distance_start) / CLOUDSAMPLES;
	float orbitview_falloff = 1.0;//smoothstep(0.0, 400.0/worldScale, 500.0/worldScale - (length(cameraPositionf) - planetRadius - seaLevel));
	for (float i=0.0; i < CLOUDSAMPLES; i+=1.0)
	{    
		const float t = cloud_distance_start + (i+0.5)*sampleStep;    
		const vec3 samplePoint = entryPoint + t*ray;
		const float altitude = length(samplePoint);
			
		float cloudensity = getCloudDensity(samplePoint, altitude - planetRadius - seaLevel, h_clouds, h_clouds + cloud_depth, cloud_depth);
		
		extinctionRayleigh += airdensity2(samplePoint, sampleStep, RAYLEIGH_SCALEHEIGHT);
		extinctionMie += airdensity2(samplePoint, sampleStep, MIE_SCALEHEIGHT);
		extinctionMie += clamp(sampleStep, 0.0, 5.0/worldScale)*(worldScale * 0.2) * cloudensity * orbitview_falloff; // note : we clamp sample Step to avoid grazing angle too big cloud density accumulation (which leads to a dark thick horizon)
		transmittance = exp2( - INVLN2 * (RAYLEIGH_COEFF * extinctionRayleigh + 1.1 * MIE_COEFF * extinctionMie));	

		vec2 exitIntersection = rayIntersectSphere(atmosphereRadius, vec3(0.0), samplePoint, lightDir);
		float exitDist = exitIntersection.y;  
		vec3 exitTransmittance = exp2(-INVLN2 *
			(
			  RAYLEIGH_COEFF  * airdensity(samplePoint, samplePoint + exitDist*lightDir, RAYLEIGH_SCALEHEIGHT, 3.0)
			  + 1.1*MIE_COEFF * airdensity(samplePoint, samplePoint + exitDist*lightDir, MIE_SCALEHEIGHT, 3.0)
			));

		float h = max(0.0, altitude - planetRadius - seaLevel * ATMOS_CORRECTION);
		vec3 rayleighScattering = RAYLEIGH_COEFF * exp2(-INVLN2 * h / RAYLEIGH_SCALEHEIGHT);
		vec3 mieScattering = vec3(MIE_COEFF * exp2(-INVLN2 * h / MIE_SCALEHEIGHT));

		vec3 sampleScatter = sampleStep * exitTransmittance * light * (rayleighPhase * rayleighScattering + miePhase * mieScattering);		
		vec3 cloudScattering = vec3(5.0 * 1.1*MIE_COEFF * cloudensity);
		sampleScatter += sampleStep * exitTransmittance * light * (cloudScattering)  * orbitview_falloff;
		
		sampleScatter *= transmittance;	
		inscatter += sampleScatter;
	}
	
	// --- sample atmosphere (part2 after clouds) ---
	sampleStep = (sampleRange - cloud_distance_end) / atmSamples_part2;
	for (float i=0.0; i < atmSamples_part2; i+=1.0)
	{    
		const float t = cloud_distance_end + (i+0.5)*sampleStep;    
		const vec3 samplePoint = entryPoint + t*ray;
		const float altitude = length(samplePoint);
			
		extinctionRayleigh += airdensity2(samplePoint, sampleStep, RAYLEIGH_SCALEHEIGHT);
		extinctionMie += airdensity2(samplePoint, sampleStep, MIE_SCALEHEIGHT);
		transmittance = exp2( - INVLN2 * (RAYLEIGH_COEFF * extinctionRayleigh + 1.1 * MIE_COEFF * extinctionMie));	

		vec2 exitIntersection = rayIntersectSphere(atmosphereRadius, vec3(0.0), samplePoint, lightDir);
		float exitDist = exitIntersection.y;  
		vec3 exitTransmittance = exp2(-INVLN2 *
			(
			  RAYLEIGH_COEFF  * airdensity(samplePoint, samplePoint + exitDist*lightDir, RAYLEIGH_SCALEHEIGHT, 3.0)
			  + 1.1*MIE_COEFF * airdensity(samplePoint, samplePoint + exitDist*lightDir, MIE_SCALEHEIGHT, 3.0)
			));

		float h = max(0.0, altitude - planetRadius - seaLevel * ATMOS_CORRECTION);
		vec3 rayleighScattering = RAYLEIGH_COEFF * exp2(-INVLN2 * h / RAYLEIGH_SCALEHEIGHT);
		vec3 mieScattering = vec3(MIE_COEFF * exp2(-INVLN2 * h / MIE_SCALEHEIGHT));

		vec3 sampleScatter = sampleStep * exitTransmittance * light * (rayleighPhase * rayleighScattering + miePhase * mieScattering);
		sampleScatter *= transmittance;	
		
		inscatter += sampleScatter;
	}
}

void atmosphereScattering(in vec3 entryPoint, in vec3 ray, in float sampleRange, in vec3 lightDir, in vec3 light
                          , out vec3 transmittance, out vec3 inscatter)
{
  const float h_clouds = 13.0 / worldScale;//13 km, cirrus
  const float cloud_depth = 0.3 / worldScale;//400m
  bool render_clouds = false;
  vec2 intersectCloudBase = rayIntersectSphere(h_clouds + planetRadius + seaLevel, vec3(0.0), entryPoint, ray);
  vec2 intersectCloudTop = rayIntersectSphere(h_clouds + cloud_depth + planetRadius + seaLevel, vec3(0.0), entryPoint, ray);
  float cloud_distance_start, cloud_distance_end;
  if (intersectCloudTop != NO_INTERSECTION && intersectCloudTop.y > 0.0)
  {
	if (intersectCloudTop.x >= 0.0)
	{
		render_clouds = true;
		cloud_distance_start = intersectCloudTop.x;
		if (intersectCloudBase == NO_INTERSECTION)
			cloud_distance_end = intersectCloudTop.y;
		else cloud_distance_end = intersectCloudBase.x;
	}
	else 
	{
		render_clouds = true;
		if (intersectCloudBase == NO_INTERSECTION)
		{
			cloud_distance_start = 0.0;
			cloud_distance_end = intersectCloudTop.y;
		}
		else if (intersectCloudBase.x >= 0.0)
		{
			cloud_distance_start = 0.0;
			cloud_distance_end = intersectCloudBase.x;
		}
		else {
			cloud_distance_start = intersectCloudBase.y;
			cloud_distance_end = intersectCloudTop.y;
		}
	}
  }     
  if (render_clouds && sampleRange > cloud_distance_start && (camera_altitude < h_clouds || camera_altitude > h_clouds + cloud_depth))
  {
	  atmosphereAndCloudsScattering(cloud_distance_start, cloud_distance_end, entryPoint, ray, sampleRange, lightDir, light, transmittance, inscatter);
	  return;
  }
  
  const float NUMSAMPLES = 12.0;
    
  inscatter = vec3(0.0);
  
  const vec3 sampleEnd = entryPoint + sampleRange * ray;
  const float sampleStep = sampleRange/NUMSAMPLES;
  
  vec3 extinctionRayleigh = vec3(0.0);
  vec3 extinctionMie = vec3(0.0);

  const float cosTheta = dot(ray, lightDir);
  const float c = 1.0 + cosTheta*cosTheta;
  const float rayleighPhase = 0.0596831 * c;
  const float miePhase = 0.050421 * c / (2.5776 * pow(1.5776 - 1.52*cosTheta, 1.5));// Cornette-Shanks phase function with g = 0.76

  for (float i=0.0; i < NUMSAMPLES; i+=1.0)
  {    
    const float t = (i+0.5)*sampleStep;    
    const vec3 samplePoint = entryPoint + t*ray;
    const float altitude = length(samplePoint);
		
	extinctionRayleigh += airdensity2(samplePoint, sampleStep, RAYLEIGH_SCALEHEIGHT);
	extinctionMie += airdensity2(samplePoint, sampleStep, MIE_SCALEHEIGHT);
	transmittance = exp2( - INVLN2 * (RAYLEIGH_COEFF * extinctionRayleigh + 1.1 * MIE_COEFF * extinctionMie));	
    
    vec2 exitIntersection = rayIntersectSphere(atmosphereRadius, vec3(0.0), samplePoint, lightDir);
    float exitDist = exitIntersection.y;  
    vec3 exitTransmittance = exp2(-INVLN2 *
        (
          RAYLEIGH_COEFF  * airdensity(samplePoint, samplePoint + exitDist*lightDir, RAYLEIGH_SCALEHEIGHT, 3.0)
          + 1.1*MIE_COEFF * airdensity(samplePoint, samplePoint + exitDist*lightDir, MIE_SCALEHEIGHT, 3.0)
        ));
    
	float h = max(0.0, altitude - planetRadius - seaLevel * ATMOS_CORRECTION);
    vec3 rayleighScattering = RAYLEIGH_COEFF * exp2(-INVLN2 * h / RAYLEIGH_SCALEHEIGHT);
    vec3 mieScattering = vec3(MIE_COEFF * exp2(-INVLN2 * h / MIE_SCALEHEIGHT));
		
	vec3 sampleScatter = sampleStep * exitTransmittance * light * (rayleighPhase * rayleighScattering + miePhase * mieScattering);
	sampleScatter *= transmittance;	
	
	inscatter += sampleScatter;
  }
}

void aerialPerspective(in vec3 entryPoint, in vec3 ray, in float sampleRange, in vec3 lightDir, in vec3 light
                        , out vec3 transmittance, out vec3 inscatter)
{
  atmosphereScattering(entryPoint, ray, sampleRange, lightDir, light, transmittance, inscatter); 
}


dvec2 getRepeatTexcoords64(dvec2 coords)
{
	return fract(coords);
}
dvec2 getMirroredRepeatTexcoords64(dvec2 coords)
{
	dvec2 ipart;//integer part
	dvec2 fpart = modf(coords, ipart);//fractional part
	ipart = abs(fract(ipart * 0.5LF) - 0.5LF);
	if (ipart.x < 0.0001LF)//odd integer:
		fpart.x = 1.0LF - fpart.x;
	if (ipart.y < 0.0001LF)//odd integer:
		fpart.y = 1.0LF - fpart.y;
	return fpart;
}

dvec4 textureFilter64Mirrored(sampler2D samp, dvec2 uv)
{
	dvec2 res = dvec2(textureSize(samp, 0) - 1.0);
	dvec2 fuv = getMirroredRepeatTexcoords64(uv) * res;
	dvec2 dcoords;
	dvec2 lerp = modf(fuv, dcoords);
	ivec2 coords = ivec2(dcoords);	
	dvec4 d00 = dvec4(texelFetch(samp, coords + ivec2(0, 0), 0));
	dvec4 d10 = dvec4(texelFetch(samp, coords + ivec2(1, 0), 0));
	dvec4 d11 = dvec4(texelFetch(samp, coords + ivec2(1, 1), 0));
	dvec4 d01 = dvec4(texelFetch(samp, coords + ivec2(0, 1), 0));
	
	return mix( mix( d00, d10, lerp.x ), mix( d01, d11, lerp.x ), lerp.y );
}

dvec4 textureFilter64(sampler2D samp, dvec2 uv)
{
	dvec2 res = dvec2(textureSize(samp, 0) - 1.0);
	dvec2 fuv = getRepeatTexcoords64(uv) * res;
	//precise dvec2 fuv = fract(uv) * res;// - 0.5LF;
	
	//precise 
	dvec2 dcoords;
	///precise 
	dvec2 lerp = modf(fuv, dcoords);
	ivec2 coords = ivec2(dcoords);	
	dvec4 d00 = dvec4(texelFetch(samp, coords + ivec2(0, 0), 0));
	dvec4 d10 = dvec4(texelFetch(samp, coords + ivec2(1, 0), 0));
	dvec4 d11 = dvec4(texelFetch(samp, coords + ivec2(1, 1), 0));
	dvec4 d01 = dvec4(texelFetch(samp, coords + ivec2(0, 1), 0));
	
	//precise dvec4 tex = mix( mix( d00, d10, lerp.x ), mix( d01, d11, lerp.x ), lerp.y );
	//return tex;
	return mix( mix( d00, d10, lerp.x ), mix( d01, d11, lerp.x ), lerp.y );
}

vec3 textureTriPlanar64MirroredRepeat(sampler2D tex, dvec3 blending, dvec3 pos)
{    
	dvec3 tx = textureFilter64Mirrored( tex, pos.yz).rgb;
    dvec3 ty = textureFilter64Mirrored( tex, pos.xz).rgb;
    dvec3 tz = textureFilter64Mirrored( tex, pos.xy).rgb;
	// blend the results of the 3 planar projections.
    //precise 
	dvec3 t = tx * blending.x + ty * blending.y + tz * blending.z;
	
    return pow(vec3(t), vec3(2.2));//gamma linearize    
}

vec3 textureTriPlanar64(sampler2D tex, dvec3 blending, dvec3 pos)
{    
	dvec3 tx = textureFilter64( tex, pos.yz).rgb;
    dvec3 ty = textureFilter64( tex, pos.xz).rgb;
    dvec3 tz = textureFilter64( tex, pos.xy).rgb;
	// blend the results of the 3 planar projections.
    //precise 
	dvec3 t = tx * blending.x + ty * blending.y + tz * blending.z;
	
    return pow(vec3(t), vec3(2.2));//gamma linearize    
}


vec3 textureTriPlanar(sampler2D tex, vec3 blending, vec3 pos)
{    
	vec3 tx = texture( tex, pos.yz).rgb;
    vec3 ty = texture( tex, pos.xz).rgb;
    vec3 tz = texture( tex, pos.xy).rgb;	
    // blend the results of the 3 planar projections.
    vec3 t = tx * blending.x + ty * blending.y + tz * blending.z;
	
    t = pow(t, vec3(2.2));//gamma linearize
    return t;
}
/*dvec4 textureFilter64Mirrored(sampler2DArray  samp, int layer, dvec2 uv)
{
	dvec2 res = dvec2(textureSize(samp, 0) - 1.0);
	dvec2 fuv = getMirroredRepeatTexcoords64(uv) * res;
	dvec2 dcoords;
	dvec2 lerp = modf(fuv, dcoords);
	ivec2 coords = ivec2(dcoords);	
	dvec4 d00 = dvec4(texelFetch(samp, ivec3(coords + ivec2(0, 0), layer), 0));
	dvec4 d10 = dvec4(texelFetch(samp, ivec3(coords + ivec2(1, 0), layer), 0));
	dvec4 d11 = dvec4(texelFetch(samp, ivec3(coords + ivec2(1, 1), layer), 0));
	dvec4 d01 = dvec4(texelFetch(samp, ivec3(coords + ivec2(0, 1), layer), 0));
	
	return mix( mix( d00, d10, lerp.x ), mix( d01, d11, lerp.x ), lerp.y );
}

dvec4 textureFilter64(sampler2DArray samp, int layer, dvec2 uv)
{
	dvec2 res = dvec2(textureSize(samp, 0) - 1.0);
	dvec2 fuv = getRepeatTexcoords64(uv) * res;
	//precise dvec2 fuv = fract(uv) * res;// - 0.5LF;
	
	//precise 
	dvec2 dcoords;
	///precise 
	dvec2 lerp = modf(fuv, dcoords);
	ivec2 coords = ivec2(dcoords);	
	dvec4 d00 = dvec4(texelFetch(samp, ivec3(coords + ivec2(0, 0), layer), 0));
	dvec4 d10 = dvec4(texelFetch(samp, ivec3(coords + ivec2(1, 0), layer), 0));
	dvec4 d11 = dvec4(texelFetch(samp, ivec3(coords + ivec2(1, 1), layer), 0));
	dvec4 d01 = dvec4(texelFetch(samp, ivec3(coords + ivec2(0, 1), layer), 0));
	
	//precise dvec4 tex = mix( mix( d00, d10, lerp.x ), mix( d01, d11, lerp.x ), lerp.y );
	//return tex;
	return mix( mix( d00, d10, lerp.x ), mix( d01, d11, lerp.x ), lerp.y );
}

vec3 textureTriPlanar64MirroredRepeat(sampler2DArray tex, int layer, dvec3 blending, dvec3 pos)
{    
	dvec3 tx = textureFilter64Mirrored( tex, layer, pos.yz).rgb;
    dvec3 ty = textureFilter64Mirrored( tex, layer, pos.xz).rgb;
    dvec3 tz = textureFilter64Mirrored( tex, layer, pos.xy).rgb;
	// blend the results of the 3 planar projections.
    //precise 
	dvec3 t = tx * blending.x + ty * blending.y + tz * blending.z;
	
    return pow(vec3(t), vec3(2.2));//gamma linearize    
}

vec3 textureTriPlanar64(sampler2DArray tex, int layer, dvec3 blending, dvec3 pos)
{    
	dvec3 tx = textureFilter64( tex, layer, pos.yz).rgb;
    dvec3 ty = textureFilter64( tex, layer, pos.xz).rgb;
    dvec3 tz = textureFilter64( tex, layer, pos.xy).rgb;
	// blend the results of the 3 planar projections.
    //precise 
	dvec3 t = tx * blending.x + ty * blending.y + tz * blending.z;
	
    return pow(vec3(t), vec3(2.2));//gamma linearize    
}


vec3 textureTriPlanar(sampler2DArray tex, int layer, vec3 blending, vec3 pos)
{    
	vec3 tx = texture( tex, vec3(pos.yz, float(layer))).rgb;
    vec3 ty = texture( tex, vec3(pos.xz, float(layer))).rgb;
    vec3 tz = texture( tex, vec3(pos.xy, float(layer))).rgb;	
    // blend the results of the 3 planar projections.
    vec3 t = tx * blending.x + ty * blending.y + tz * blending.z;
	
    t = pow(t, vec3(2.2));//gamma linearize
    return t;
}*/

vec3 getTexturedTerrainColor(vec3 worldPos, dvec3 posKm, float altitude, vec3 normal, vec4 terrainInfo, inout float specpower, inout float specamplitude)
{
	vec3 color;		
	
	const float tectonicAge = smoothstep(0.0, 200.0, terrainInfo.x);
	const float desert = terrainInfo.z;
    const float terrainDistanceKm = distance(cameraPositionf, worldPos)*worldScale;
	const vec3 normedWorldPos = normalize(worldPos);
    const float latitude = normedWorldPos.z * normedWorldPos.z;//latitude encoding
	
    // TRIPLANAR TEXTURING:
    vec3 blending = abs( normal );
    blending = normalize(max(blending, 0.00001)); // Force weights to sum to 1.0
    float b = (blending.x + blending.y + blending.z);
    blending /= vec3(b);    
	
	const vec3 pos = worldPos * worldScale * 0.1;
	
	precise dvec3 detailPos = posKm;
	precise dvec3 hiDetailPos = 24.0LF * posKm;		
	precise dvec3 dblending = dvec3(blending);//dvec3(0.0LF, 0.0LF, 1.0LF);////abs( normalize(posKm) );
	//double db = dblending.x + dblending.y + dblending.z;
	//dblending /= dvec3(db);
	
	vec3 midPos = pos * 0.25;
	vec3 lowPos = pos / 256.0;
	const float lowLerp = smoothstep(40.0, 160.0, terrainDistanceKm);
	const float detailLerp = smoothstep(0.5, 13.0, terrainDistanceKm);
	const float hiDetailLerp = smoothstep(0.1, 0.382, terrainDistanceKm);
	const float detailNoise = texture(noiseTexture, 2.0*vec3(detailPos)).r;	       
	
	//vec3 lowRockColor = textureTriPlanar(terrainTexture, TEXTURE_LAYER_ROCK, blending, lowPos);
	vec3 lowRockColor = textureTriPlanar(rockTexture, blending, lowPos);
    vec3 rockColor = textureTriPlanar(rockTexture, blending, midPos);
    vec3 detailRock = textureTriPlanar64(rockTexture, dblending, detailPos);      
	vec3 hiDetailRock = smoothstep(0.0, 1.0, textureTriPlanar64(rockTexture, dblending, hiDetailPos));
	//vec3 hiDetailRock = textureTriPlanar64(rockTexture, dblending, hiDetailPos);
    rockColor = mix(rockColor, lowRockColor, lowLerp);            
    rockColor = mix(0.7*detailRock + 0.3*rockColor, rockColor, detailLerp);
	rockColor = mix(0.8*hiDetailRock+0.2*rockColor, rockColor, hiDetailLerp);
	//rockColor = mix(1.0*hiDetailRock+0.0*rockColor, rockColor, hiDetailLerp);
	float rockValue = 0.333333 * (rockColor.r + rockColor.g + rockColor.b);
	rockColor = mix(rockColor, vec3(0.618, 0.382, 0.18), 0.5*desert);//make rocks red when in desert
	
	vec3 lowDesertColor = textureTriPlanar(desertTexture, blending, lowPos);
	vec3 desertColor = textureTriPlanar(desertTexture, blending, midPos);
	vec3 detailDesert = textureTriPlanar64(desertTexture, dblending, detailPos);
	desertColor = mix(0.7*desertColor + 0.3*lowDesertColor, lowDesertColor, lowLerp);
	desertColor = mix(0.7*detailDesert + 0.3*desertColor, desertColor, detailLerp);
	
	vec3 lowGrassColor = textureTriPlanar(grassTexture, blending, lowPos);
	vec3 grassColor = textureTriPlanar(grassTexture, blending, midPos);
	vec3 detailGrass = textureTriPlanar64(grassTexture, dblending, detailPos);
	vec3 hiDetailGrass = textureTriPlanar64(grassTexture, dblending, hiDetailPos);
	const float grasslerp = smoothstep(1.2, 2.0, altitude * worldScale);//smoothstep(0.35, 0.65, texture(noiseTexture, pos * 0.1).r);//smoothstep(1.4 + 0.6 * texture(noiseTexture, midPos).r, 2.4, altitude * worldScale);
	const vec3 enhancegrass = mix(vec3(0.75, 1.1, 0.42), vec3(1.0), grasslerp);
	lowGrassColor *= enhancegrass;
	grassColor *= enhancegrass;
	detailGrass *= enhancegrass;
	//hiDetailGrass *= enhancegrass;
	grassColor = mix(0.7*grassColor + 0.3*lowGrassColor, lowGrassColor, lowLerp);
	grassColor = mix(0.7*detailGrass + 0.3*grassColor, grassColor, detailLerp);
    grassColor = mix(0.9*hiDetailGrass + 0.1*grassColor, grassColor, hiDetailLerp);      
	
	// BIOMES:
	//float noise = texture(noiseTexture, vec3(8.8) + pos * 0.001).r;
	//float lat = abs(pos.z) / 637.0 + 0.16 * (-1.0 + 2.0*noise);
	//float biomeDistrib = smoothstep(0.16, 0.3, lat) * (1.0 - smoothstep(0.35, 0.49, lat));
	//const float biomelerp = biomeDistrib * smoothstep(0.5, 0.6, texture(noiseTexture, vec3(17.7) + pos * 0.00025).r);
	const float desertBand = 1.0 - smoothstep(0.47, 0.6, latitude);
	const float biomelerp = desertBand * desert;//biomeDistrib * desert;
	//desertColor = mix(desertColor, vec3(0.8, 0.78, 0.71), 0.38*tectonicAge);// TO DO (for future sand deserts)
	vec3 biomeColor = mix(grassColor, desertColor, biomelerp);
	specamplitude = mix(0.03, 0.14, biomelerp);
	specpower = mix(2.0, 4.0, biomelerp);
	
	// BASE ROCK:
	float rockAltitude = (4.0 - 2.2 * latitude - 4.0 * desert * (1.0 - tectonicAge) - 0.6*texture(noiseTexture, 0.1*pos).r);
	float rlerp = clamp(altitude*worldScale - rockAltitude, 0.0, 0.25) * 4.0;
	color = mix(biomeColor, rockColor, rlerp);
	specamplitude = mix(specamplitude, 0.09, rlerp);
	specpower = mix(specpower, 64.0*(1.0 - rockValue), rlerp);
	// slope effect
    float rockthreshold = 0.8 + 0.2 * texture(noiseTexture, midPos).r;
    float slope = 1.0 - dot(normedWorldPos, normal);
	float rocklerp = (1.0 - slope)/rockthreshold;
	rocklerp *= rocklerp;
	rocklerp *= rocklerp;	
	rocklerp = smoothstep(0.0, 1.0, rocklerp);
    color = mix(rockColor, color, rocklerp);
	specamplitude = mix(0.09, specamplitude, rocklerp);
	specpower = mix(64.0*(1.0 - rockValue), specpower, rocklerp);
	
	// SNOW & ICe:
	if (deactivateSnowFx == 0)
	{
		vec3 lowSnowColor = textureTriPlanar(snowTexture, blending, lowPos);
		vec3 snowColor = textureTriPlanar(snowTexture, blending, midPos);
		vec3 detailSnow = textureTriPlanar64(snowTexture, dblending, detailPos);
		snowColor = mix(0.7*snowColor + 0.3*lowSnowColor, lowSnowColor, lowLerp);
		snowColor = mix(0.7*detailSnow + 0.3*snowColor, snowColor, detailLerp);
		snowColor *= 0.85;
		float snowCoverNoise = 0.5* texture(noiseTexture, midPos).r + 0.5* texture(noiseTexture, pos).r;
		snowCoverNoise = smoothstep(0.1, 0.9, snowCoverNoise) * (1.0 - smoothstep(0.7, 0.8, latitude));// * sqrt(slope);
		float snowAltitude = (3.8 + 2.2 * snowCoverNoise - 5.0 * latitude * (1.0 - desert))/worldScale;
		//const vec3 snowColor = vec3(0.93, 0.96, 0.98);
		float snowlerp = smoothstep(0.7, 0.73, 1.0 - slope)*step(0.0, altitude - snowAltitude) * step(0.382, 1.0 - desert);
		color = mix(color, snowColor, snowlerp);
		specamplitude = mix(specamplitude, 0.5, snowlerp);	
		specpower = mix(specpower, 80.0, snowlerp);
	}
	
	return color;
}


//
///
////
/////
//////
///////
/////////
void main()
{
	const vec3 gamma = vec3(1.0/2.2);//gamma correction
	//const vec3 gamma = vec3(1.0/1.96);//gamma correction

	vec3 ray = normalize(proxyPos - cameraPositionf);	
	vec3 color;
	vec3 transmittance;
	vec3 inscatter;    
	const bool submarineView = camera_altitude < 0.0;	

	// Space background:
	vec3 spaceBgColor = vec3(0.0);
	const vec3 suncenter = SUN_DIR * 600.0;
	const float sunradius = 8.0;
	const float suncoronaradius = 8.0*sunradius;
	vec2 sx = rayIntersectSphere(suncoronaradius, suncenter, cameraPositionf, ray);
	if (sx != NO_INTERSECTION && sx.x > 0.0)
	{
	  vec3 cordp = 0.5 * (2.0*cameraPositionf + ray * (sx.x + sx.y));
	  float r = distance(cordp, suncenter);
	  spaceBgColor = 0.382*SUN_LIGHT * smoothstep(0.1, 1.0, exp2(-INVLN2 * r/suncoronaradius * 8.0));
	}

	// fetch geometry buffer:
	ivec2 buffercoords = ivec2(gl_FragCoord.xy - 0.5);
	const vec4 terrainGeometry0 = texelFetch(terrainGeometryBuffer0, buffercoords, 0);    
	const vec3 terrainPos = terrainGeometry0.xyz;  	
	const float terrainAltitude = terrainGeometry0.w;  
	const vec4 terrainGeometry1 = texelFetch(terrainGeometryBuffer1, buffercoords, 0);  
	const vec3 terrainNormal = terrainGeometry1.xyz;
	const float terrainDistance2Camf = terrainGeometry1.w;
	const bool terrainPresence = (terrainPos != vec3(0.0));
	vec4 terrainInfo = vec4(0.0);//x: 1 = desert
	
	const vec4 waterGeometry0 = texelFetch(waterGeometryBuffer0, buffercoords, 0);
	const vec3 waterPos = waterGeometry0.xyz;  
	const float waterAltitude = waterGeometry0.w;	
	const vec4 waterGeometry1 = texelFetch(waterGeometryBuffer1, buffercoords, 0);
	vec3 waterNormal = waterGeometry1.xyz; //normalize(waterPos);// DEBUG // 
	const float waterDistance2Camf = waterGeometry1.w;
	bool waterPresence = (waterPos != vec3(0.0));
	if (terrainPresence)
		waterPresence = waterPresence && (terrainDistance2Camf > waterDistance2Camf);//if terrain is present, to see water, the water distance have to be shorter than the terrain distance to camera.	
	WATER_EXTINCTION_RGB = mix(SEAWATER_EXTINCTION_RGB, RIVER_EXTINCTION_RGB, smoothstep(0.0, 0.1, worldScale * waterAltitude));//modulate appearance of water between sea water and river water (based on water altitude)
	//WATER_EXTINCTION_RGB = RIVER_EXTINCTION_RGB;

	// ray/sphere intersection:
	const dvec3 ray_d = dvec3(ray);
	vec3 cam_offset = cameraPositionf;
	vec2 t;
	const dvec2 _t = _rayIntersectSphere(double(atmosphereRadius), dvec3(0.0LF), cameraPosition, ray_d);
	if (_t.x > 0.0LF)
	{
		cam_offset += vec3(0.96LF * _t.x * ray_d);
		t = rayIntersectSphere(atmosphereRadius, vec3(0.0), cam_offset, ray);
	} else
		t = vec2(_t);

	// ****************** no geometry hit ****************** 
	if (!terrainPresence && !waterPresence)
	{
		if (t == NO_INTERSECTION || (t.x < 0.0 && t.y < 0.0))
		{
		  color = clamp(spaceBgColor, 0.0, 0.17) + spaceBgColor;//empty space
		  color = pow(clamp(color, 0.0, 1.0), gamma);  
		  outColor = vec4(color, 1.0);
		  return;
		}

		t.x = max(0.0, t.x);
		vec3 trans, scatt;
		atmosphereScattering(cam_offset + t.x*ray, ray, t.y - t.x, SUN_DIR, SUN_LIGHT, trans, scatt);//atmosphere only
		color = clamp(spaceBgColor, 0.0, 0.17) + trans * spaceBgColor + scatt;
		if (!submarineView)
		{
			color = pow(color, vec3(gamma));  
			outColor = clamp(vec4(color, 1.0), 0.0, 1.0);
			return;
		}
	}
	
	//const bool waterVisible = waterPresence && (!terrainPresence || terrainAltitude < waterAltitude);    
	const vec3 H = normalize(-ray + SUN_DIR);
	const float cosH = max(0.0, dot(terrainNormal, H));
	const float cosNL = max(0.0, dot(terrainNormal, SUN_DIR));
	precise dvec3 posKm;
	if (terrainPresence)
	{
		const dvec2 pixel = -1.0LF + 2.0LF * dvec2(gl_FragCoord.xy - 0.5) / (screenSizePixels - 1.0LF);
		const dvec4 dScreenPos = inverseViewProj * dvec4(pixel, -1.0LF, 1.0LF);
		const dvec3 dRay = normalize(dScreenPos.xyz / dScreenPos.w - cameraPosition);
		posKm = renderScale * (cameraPosition + dRay * double(terrainDistance2Camf));
		//posKm = dvec3(terrainPos) * renderScale; // DEBUG
		terrainInfo = texelFetch(terrainGeometryBuffer2, buffercoords, 0);  
	}
	
	// ****************** Shade Terrain ******************
	if (terrainPresence && !waterPresence && !submarineView)// && !waterVisible)
	{				
		float specpower = 1.0, specamplitude = 0.0;
		color = getTexturedTerrainColor(terrainPos, posKm, terrainAltitude, terrainNormal, terrainInfo, specpower, specamplitude);
				
		// incoming light
		vec2 terraint = rayIntersectSphere(atmosphereRadius, vec3(0.0), terrainPos, SUN_DIR);
		aerialPerspective(terrainPos, SUN_DIR, terraint.y, SUN_DIR, 1.75*SUN_LIGHT, transmittance, inscatter);
		color = color * 0.001 + (transmittance * SUN_COLOR + inscatter) * ( color * cosNL + specamplitude * pow(cosH, specpower) );   

		// aerial perspective 
		vec3 entry = max(0.0, t.x) * ray + cam_offset;
		aerialPerspective(entry, ray, distance(terrainPos, entry), SUN_DIR, 0.618 * SUN_LIGHT, transmittance, inscatter);    
		color = color * transmittance + inscatter;      
		
		//color = vec3(0.0, 1.0, 0.0);//debug (to detect cracks in water geometry)
	} 
	// ****************** Shade Water ******************
	else if (waterPresence && !submarineView)
	{	
		float waterDepth = 10.0/worldScale;	
		vec3 terrainColor = vec3(0.382);
		float lightWaterDepth = waterDepth;
		if (terrainPresence)
		{
			float specpower = 1.0, specamplitude = 0.0;
			terrainColor = getTexturedTerrainColor(terrainPos, posKm, terrainAltitude, terrainNormal, terrainInfo, specpower, specamplitude);			
			waterDepth = distance(waterPos, terrainPos);
			lightWaterDepth = rayIntersectSphere(waterAltitude + planetRadius + seaLevel, vec3(0.0), terrainPos, SUN_DIR).y;// this is approximate for rivers (not for oceans) !
		}  	
		
		const vec3 lightWaterTransmittance = exp2(-INVLN2 * WATER_EXTINCTION_RGB * lightWaterDepth * waterArtificialOpacity);
		terrainColor *= 0.001 + SUN_COLOR * cosNL * lightWaterTransmittance;		
		const vec3 waterTransmittance = exp2(-INVLN2 * WATER_EXTINCTION_RGB * waterDepth * waterArtificialOpacity);

		const vec3 reflectRay = reflect(ray, waterNormal);
		vec2 bounds = rayIntersectSphere(atmosphereRadius, vec3(0.0), waterPos, reflectRay);
		vec3 reflectColor = vec3(0.0);
		//if (bounds.y < abs(bounds.x))
		//{
			
		//if (dot(normalize(waterPos), reflectRay) > 0.0)	
		//{
			aerialPerspective(waterPos, reflectRay, bounds.y, SUN_DIR, SUN_LIGHT, transmittance, inscatter);
			reflectColor = inscatter;// + clamp(spaceBgColor, 0.0, 0.17) + transmittance * spaceBgColor ;
		//} 
		//else reflectColor = vec3(0.0, 0.2, 0.4);//waterTransmittance;
		//} else {
		  //aerialPerspective(waterPos, -ray, abs(bounds.x), SUN_DIR, SUN_LIGHT, transmittance, inscatter);
		  //reflectColor = inscatter;//clamp(spaceBgColor, 0.0, 0.17) + transmittance * spaceBgColor +
		//}
		
		const float waterCosNL = max(0.0, dot(waterNormal, SUN_DIR));    
		//const float waterCosNL = max(0.0, dot(terrainNormal, SUN_DIR));    
		color = (vec3(0.0, 0.05, 0.1) + waterTransmittance * terrainColor) * waterCosNL + 0.38*reflectColor;

		// enhance contrast artificially:
		color = smoothstep(0.0, 1.0, color);
		//
		vec3 entry = max(0.0, t.x) * ray + cam_offset;
		aerialPerspective(entry, ray, distance(waterPos, entry), SUN_DIR, 0.618*SUN_LIGHT, transmittance, inscatter);    
		color = color * transmittance + inscatter;  
	}

	// ****************** Shade Underwater ****************** 
	else 
	{	
		float waterDepth = 10.0/worldScale;	
		vec3 terrainColor = vec3(0.2);
		float lightWaterDepth = waterDepth;
		if (terrainPresence)
		{
			float specpower = 1.0, specamplitude = 0.0;
			terrainColor = getTexturedTerrainColor(terrainPos, posKm, terrainAltitude, terrainNormal, terrainInfo, specpower, specamplitude);			
			waterDepth = terrainDistance2Camf;
			lightWaterDepth = rayIntersectSphere(waterAltitude + planetRadius + seaLevel, vec3(0.0), terrainPos, SUN_DIR).y;
			vec3 lightWaterTransmittance = exp(-INVLN2 * WATER_EXTINCTION_RGB * lightWaterDepth * waterArtificialOpacity);
			terrainColor *= 0.001 + 0.1*SUN_LIGHT * cosNL * lightWaterTransmittance;	
		}  	
		else {
			terrainColor = color;//atmos inscatter
			waterDepth = rayIntersectSphere(waterAltitude + planetRadius + seaLevel, vec3(0.0), cameraPositionf, ray).y;		
		}	
				
		vec3 waterTransmittance = exp2(-INVLN2 * WATER_EXTINCTION_RGB * waterDepth * waterArtificialOpacity);       
		color = vec3(0.0, 0.05, 0.1) + waterTransmittance * terrainColor;
	}


	// - exposure - 
	//const float exposure = 1.5;
	//color.rgb = 1.0 - exp2(- exposure * color.rgb);

	// - gamma correction -
	color = clamp(color, 0.0, 1.0);    
	color = pow(color, gamma);  
	
	// - contrast -
	color = mix(color, smoothstep(0.0, 1.0, color), 0.618);
	/*color.r = mix(color.r, smoothstep(0.0, 1.0, color.r), 0.382);
	color.g = mix(color.g, smoothstep(0.0, 1.0, color.g), 0.3);
	color.b = mix(color.b, smoothstep(0.0, 1.0, color.b), 0.2);*/
	
	// - enhance dark hues -
	//color = pow(color, vec3(1.18));
	
	outColor = vec4(color, 1.0);
}
