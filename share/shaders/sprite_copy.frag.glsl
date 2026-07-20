#version 150 core

in vec2 tex;
in vec4 col;
out vec4 outColor;

uniform sampler2D tex0;
uniform vec4 tint;
uniform float alpha;
uniform vec4 params0;
uniform vec4 params1;

void main() {
  vec4 sampled = texture(tex0, tex) * col;
  vec4 src = params0.x > 0.5 ? vec4(params1.rgb, sampled.a * params1.a)
                             : sampled * tint;
  outColor = src;
  outColor.a *= alpha;
  if (outColor.a < 0.1) discard;
}
