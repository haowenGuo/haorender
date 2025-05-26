
#include"shader.h"
Shader::~Shader() {};
void SimpleShader::addLight(Vec4f light) { light_dirs.push_back(light); }
void SimpleShader::setUV(vector<Vec2f>& uv0) { uvs = uv0; }
void SimpleShader::setNormal(vector<Vec4f>& normal0) { normals = normal0; }
void SimpleShader::setTgatexture(const shared_ptr<TGAImage>& tgatext) { tgatexture =tgatext; }
void SimpleShader::setNMTexture(const shared_ptr<TGAImage>& tgatext) { nmtexture = tgatext; }
void SimpleShader::setSpecTexture(const shared_ptr<TGAImage>&tgatext) { spectexture = tgatext; }
void SimpleShader::setUniform_M(const Mat& m) {
	uniform_M = m; 
	uniform_MIT= uniform_M.inv().t();
};
void SimpleShader::setUniform_MIT(const Mat& m) { uniform_MIT = m; };
void SimpleShader::setUniform_V(const Mat& m) { uniform_V = m; };
//void SimpleShader::setPointions(vector<Vec4f>& position0) { positions = position0; };
vec4 SimpleShader::vertexShader(const vertex& vertexs, const Mat& mvp) {

	//vec4 ves_bymvp = vertexs.v * mvp;
	//return ves_bymvp;
	return vec4();
};
int SimpleShader::fragmentShader(Vec3f& bc, Vec3b& color) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 5.f, spec = 16.f;
	Vec4f n(0.f, 0.f, 0.f, 0.f);
	Vec4f col(255.f, 255.f, 255.f, 0.f);

	for (int i = 0; i < 3; i++) {
		u += bc[i] * uvs[i][0];
		v += bc[i] * uvs[i][1];
		n[0] += bc[i] * normals[i][0];
		n[1] += bc[i] * normals[i][1];
		n[2] += bc[i] * normals[i][2];
	}

	if(tgatexture){ 
		TGAColor& coltex = tgatexture->get(u * tgatexture->get_width(), v * tgatexture->get_height()); 
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}
	if (nmtexture) {
		TGAColor& col_nm = nmtexture->get(u * nmtexture->get_width(), v * nmtexture->get_height());
		n = Vec4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
	}
	if (spectexture) {
		TGAColor& col_spec = spectexture->get(u * spectexture->get_width(), v * spectexture->get_height());
		spec = col_spec.b / 1.f;
	}

	Mat result = uniform_MIT * Mat(n); 
	n = Vec4f(result.at<float>(0, 0),result.at<float>(1, 0),result.at<float>(2, 0),0.f);
	Vec4f l;
	normalize(n, n);
	for (int i = 0; i < light_dirs.size(); i++) {
		Mat m = uniform_V * light_dirs[i];
		l=Vec4f(m.at<float>(0, 0), m.at<float>(1, 0), m.at<float>(2, 0), 0.f) ;
		normalize(l, l);
		Vec4f r = l - 2.f * (n.dot(l)) * n;
		normalize(r, r);
		intense_diff += max(-n.dot(l), 0.f);
		intense_spec += pow(max(r[2], 0.0f), spec);
	}
	color = Vec3b(min(ambient + col[0]* (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[1] * (intense_diff + 0.6f * intense_spec),255.0f),
		min(ambient + col[2] * (intense_diff+ 0.6f * intense_spec),255.0f));

	return 1;
};


void ComplexShader::addLight(Vector4f light) { light_dirs.push_back(light); }
void ComplexShader::setUV(vector<Vector2f>& uv0) { uvs = uv0; }
void ComplexShader::setNormal(vector<Vector4f>& normal0) { normals = normal0; }
void ComplexShader::setUV0(const Vector2f& uv0, int index) { uv[index]=uv0; }
void ComplexShader::setN(const Vector4f& n0, int index) { n[index] = n0; }
void ComplexShader::setP(const Vector4f& p0, int index) { position[index] = p0; }
void ComplexShader::setTexture(const vector<MyImage>& textures0) { textures = textures0; }

void ComplexShader::setUniform_M(const Matrix4f& m) {
	uniform_M = m;
	uniform_MIT = uniform_M.inverse().transpose();
};
void ComplexShader::setUniform_MIT(const Matrix4f& m) { uniform_MIT = m; };
void ComplexShader::setUniform_V(const Matrix4f& m) { uniform_V = m; };
//void ComplexShader::setPointions(vector<Vector4f>& position0) { positions = position0; };
Vector4f ComplexShader::vertexShader(const vertex& vertexs, const Matrix4f& mvp) {

	Vector4f ves_bymvp =  mvp * vertexs.v;
	return ves_bymvp;
};
int ComplexShader::vertexShader2(const Vector4f& position0, const Vector2f& uv0,const Vector4f& n0, const Matrix4f& mvp,int t) {
	mvp* position0;
	positions[t].noalias() = mvp * position0;
	//cout << "22      " << position << endl;
	float w = positions[t][3];
	positions[t].noalias() = positions[t] / w;
	//cout << "33      " << position << endl;
	uvs[t] = uv0;
	normals[t] = uniform_MIT * n0;
	normals[t][3] = 0.f ;
	normals[t].normalize();
	return 1;
};
int ComplexShader::fragmentShader(Vector3f& bc, Vec3b& color,int difftexture, int nmtexture ,int spectexture) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 30.f, spec = 16.f;
	Vector4f n(0.f, 0.f, 0.f, 0.f);
	Vector4f col(255.f, 255.f, 255.f, 0.f);

	for (int i = 0; i < 3; i++) {
		u += bc[i] * uvs[i][0];
		v += bc[i] * uvs[i][1];
		n[0] += bc[i] * normals[i][0];
		n[1] += bc[i] * normals[i][1];
		n[2] += bc[i] * normals[i][2];
	}

	if (difftexture>=0) {
		MyColor& coltex = textures[difftexture].get(u * textures[difftexture].get_width(), v * textures[difftexture].get_height());
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}
	if (nmtexture >= 0) {
		MyColor& col_nm = textures[nmtexture].get(u * textures[nmtexture].get_width(), v * textures[nmtexture].get_height());
		n = Vector4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
	}
	if (spectexture >= 0) {
		MyColor& col_spec = textures[spectexture].get(u * textures[spectexture].get_width(), v * textures[spectexture].get_height());
		spec = col_spec.b / 1.f;
	}
	
	/*Mat result = uniform_MIT * Mat(n);
	n = Vec4f(result.at<float>(0, 0), result.at<float>(1, 0), result.at<float>(2, 0), 0.f);
	Vec4f l;
	normalize(n, n);
	for (int i = 0; i < light_dirs.size(); i++) {
		Mat m = uniform_V * light_dirs[i];
		l = Vec4f(m.at<float>(0, 0), m.at<float>(1, 0), m.at<float>(2, 0), 0.f);
		normalize(l, l);
		Vec4f r = l - 2.f * (n.dot(l)) * n;
		normalize(r, r);
		intense_diff += max(-n.dot(l), 0.f);
		intense_spec += pow(max(r[2], 0.0f), spec);
	}
	*/
	color = Vec3b(min(ambient + col[0] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[1] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[2] * (intense_diff + 0.6f * intense_spec), 255.0f));

	return 1;
};

int ComplexShader::fragmentShader2(Vector3f& bc, Vec3b& color, int difftexture, int nmtexture, int spectexture) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 30.f, spec = 16.f;
	Vector4f n0(0.f, 0.f, 0.f, 0.f);
	Vector4f col(255.f, 255.f, 255.f, 0.f);

	for (int i = 0; i < 3; i++) {
		u += bc[i] * uv[i][0];
		v += bc[i] * uv[i][1];
		n0[0] += bc[i] * n[i][0];
		n0[1] += bc[i] * n[i][1];
		n0[2] += bc[i] * n[i][2];
	}
	
	if (difftexture >= 0) {
		
		MyColor& coltex = textures[difftexture].get(u * textures[difftexture].get_width(), v * textures[difftexture].get_height());
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}
	/*
	if (nmtexture >= 0) {
		MyColor& col_nm = textures[nmtexture].get(u * textures[nmtexture].get_width(), v * textures[nmtexture].get_height());
		n = Vector4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
	}
	if (spectexture >= 0) {
		MyColor& col_spec = textures[spectexture].get(u * textures[spectexture].get_width(), v * textures[spectexture].get_height());
		spec = col_spec.b / 1.f;
	}*/

	//Vec4f l;

	for (int i = 0; i < light_dirs.size(); i++) {

		Vector4f r = light_dirs[i] - 2.f * (n0.dot(light_dirs[i])) * n0;
		r.normalize();
		intense_diff += max(-n0.dot(light_dirs[i]), 0.f);
		intense_spec += pow(max(r[2], 0.0f), spec);
	}
	
	/*color = Vec3b(min(ambient + col[0] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[1] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[2] * (intense_diff + 0.6f * intense_spec), 255.0f));*/
	color = Vec3b(col[0]*100,col[1] * 100,col[2] * 100);
	return 1;
};
int ComplexShader::fragmentShader3(int x, int y, int z, Vec3b& color, int difftexture, int nmtexture, int spectexture,int indexs[3]) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 30.f, spec = 16.f;
	Vector4f n0(0.f, 0.f, 0.f, 0.f);
	Vector4f col(255.f, 255.f, 255.f, 0.f);

	u = x * uvs[indexs[0]][0] + y * uvs[indexs[1]][0] + z * uvs[indexs[2]][0];
	v = x * uvs[indexs[0]][1] + y * uvs[indexs[1]][1] + z * uvs[indexs[2]][1];
	n0[0] = x*normals[indexs[0]][0] + y * normals[indexs[1]][0] + z * normals[indexs[2]][0];
	n0[1] = x * normals[indexs[0]][1] + y * normals[indexs[1]][1] + z * normals[indexs[2]][1];
	n0[2] = x * normals[indexs[0]][2] + y * normals[indexs[1]][2] + z * normals[indexs[2]][2];
	if (difftexture >= 0) {
		//cout << x << " " << y << " " << z << endl;
		MyColor& coltex = textures[difftexture].get(u * textures[difftexture].get_width(), v * textures[difftexture].get_height());
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}
	/*
	if (nmtexture >= 0) {
		MyColor& col_nm = textures[nmtexture].get(u * textures[nmtexture].get_width(), v * textures[nmtexture].get_height());
		n = Vector4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
	}
	if (spectexture >= 0) {
		MyColor& col_spec = textures[spectexture].get(u * textures[spectexture].get_width(), v * textures[spectexture].get_height());
		spec = col_spec.b / 1.f;
	}*/

	//Vec4f l;

	for (int i = 0; i < light_dirs.size(); i++) {

		Vector4f r = light_dirs[i] - 2.f * (n0.dot(light_dirs[i])) * n0;
		r.normalize();
		intense_diff += max(-n0.dot(light_dirs[i]), 0.f);
		intense_spec += pow(max(r[2], 0.0f), spec);
	}

	color = Vec3b(min(ambient + col[0] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[1] * (intense_diff + 0.6f * intense_spec), 255.0f),
		min(ambient + col[2] * (intense_diff + 0.6f * intense_spec), 255.0f));
	//color = Vec3b(col[0], col[1], col[2]);
	return 1;
};

