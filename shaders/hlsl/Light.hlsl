/* Not being used yet*/
struct LightData
{
    float3 color;
    float3 direction
}

/* Not being used yet*/
LightData ComputeLightData(PointLight pL);

struct PointLight
{
    float3 position;
    float3 color
}

