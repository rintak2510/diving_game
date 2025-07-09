#version 330

// Attribute変数 / Attribute variables
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

// Varuing変数 / Varying variables
out vec3 f_positionCameraSpace;
out vec3 f_normalCameraSpace;
out vec3 f_lightPosCameraSpace;
out vec4 v_color;

// 光源の情報 / Light parameters
uniform vec3 u_lightPos;
uniform vec3 u_diffColor;
uniform vec3 u_specColor;
uniform vec3 u_ambiColor;
uniform float u_shininess;

// 各種変換行列 / Transformation matrices
uniform mat4 u_mvMat;
uniform mat4 u_mvpMat;
uniform mat4 u_normMat;
uniform mat4 u_lightMat;

void main() {
    // gl_Positionは必ず指定する
    // You MUST specify gl_Position
    gl_Position = u_mvpMat * vec4(in_position, 1.0);

    // カメラ座標系への変換
    // Transform world-space parameters to those in the camera-space
    f_positionCameraSpace = (u_mvMat * vec4(in_position, 1.0)).xyz;
    f_normalCameraSpace = (u_normMat * vec4(in_normal, 0.0)).xyz;
    f_lightPosCameraSpace = (u_lightMat * vec4(u_lightPos, 1.0)).xyz;
    
    vec3 V = normalize(-f_positionCameraSpace);
    vec3 L = normalize(f_lightPosCameraSpace - f_positionCameraSpace);
    vec3 H = normalize(V + L);
    vec3 normal = normalize(f_normalCameraSpace);
    float ndotl = max(dot(normal, L), 0.0);
    float ndoth = max(dot(normal, H), 0.0);

    vec3 diffuse = u_diffColor * ndotl;
    vec3 specular = u_specColor * pow(ndoth, u_shininess);
    vec3 ambient = u_ambiColor;

    v_color = vec4(diffuse + specular + ambient, 1.0);
}
