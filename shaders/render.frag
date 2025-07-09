#version 330

// Varying変数: 頂点シェーダから受け取った値
// Varying variables: received from a vertex shader.
in vec4 v_color;
// Varying変数: ディスプレイへの出力変数
// Varying variables: here for exporting colors to the display
out vec4 out_color;

void main() {
    // 描画色を代入 / Store pixel colors
    out_color = v_color;
}
