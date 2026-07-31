void skinRows(float4 weights, float4 joints, uniform float4 palette[180],
              out float4 row0, out float4 row1, out float4 row2)
{
  float weight = weights.x;
  int joint = (int)joints.x * 3;
  row0 = palette[joint] * weight;
  row1 = palette[joint + 1] * weight;
  row2 = palette[joint + 2] * weight;
  weight = weights.y;
  if(weight > 0.0) {
    joint = (int)joints.y * 3;
    row0 += palette[joint] * weight;
    row1 += palette[joint + 1] * weight;
    row2 += palette[joint + 2] * weight;
    weight = weights.z;
    if(weight > 0.0) {
      joint = (int)joints.z * 3;
      row0 += palette[joint] * weight;
      row1 += palette[joint + 1] * weight;
      row2 += palette[joint + 2] * weight;
      weight = weights.w;
      if(weight > 0.0) {
        joint = (int)joints.w * 3;
        row0 += palette[joint] * weight;
        row1 += palette[joint + 1] * weight;
        row2 += palette[joint + 2] * weight;
      }
    }
  }
}
