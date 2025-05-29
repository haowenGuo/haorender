
#include"shader.h"
#include <drawer.h>
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
};
void ComplexShader::setUniform_MIT(const Matrix4f& m) { uniform_MIT = (uniform_V * uniform_M).inverse().transpose(); }
void ComplexShader::setUniform_V(const Matrix4f& m) { uniform_V = m; }
void ComplexShader::setUniform_P(const Matrix4f& p) { uniform_P = p; }
//void ComplexShader::setPointions(vector<Vector4f>& position0) { positions = position0; };
Vector4f ComplexShader::vertexShader(const vertex& vertexs, const Matrix4f& mvp) {

	Vector4f ves_bymvp =  mvp * vertexs.v;
	return ves_bymvp;
};
int ComplexShader::drawTriagle_completed(Mat& im, MatrixXd& zbuff, int difftexture, int nmtexture, int spectexture, int indexs[3],float z1, float z2, float z3) {
	/*
	if (vertexs.size() > 3 || vertexs.size() <= 0) {
		cout << "drawTriagle 异常" << endl;
		return -1;
	}*/

	int weight = im.cols;
	int height = im.rows;
	Vector4f v0 = positions[indexs[0]], v1 = positions[indexs[1]], v2 = positions[indexs[2]];
	//v0[2] = z0; v1[2] = z1; v2[2] = z2;
	int minx = weight - 1, miny = height - 1, maxx = 0, maxy = 0;
	{
		minx = max(0, min(minx, (int)v0[0])); minx = max(0, min(minx, (int)v1[0])); minx = max(0, min(minx, (int)v2[0]));
		miny = max(0, min(miny, (int)v0[1])); miny = max(0, min(miny, (int)v1[1])); miny = max(0, min(miny, (int)v2[1]));
		maxx = min(weight - 1, max(maxx, static_cast<int>(ceil(v0[0])))); maxx = min(weight - 1, max(maxx, static_cast<int>(ceil(v1[0])))); maxx = min(weight - 1, max(maxx, static_cast<int>(ceil(v2[0]))));
		maxy = min(height - 1, max(maxy, static_cast<int>(ceil(v0[1])))); maxy = min(height - 1, max(maxy, static_cast<int>(ceil(v1[1])))); maxy = min(height - 1, max(maxy, static_cast<int>(ceil(v2[1]))));
	}

	for (int x = minx; x <= maxx; x++) {
		for (int y = miny; y <= maxy; y++) {
			
			
			float x0 = x, y0 = y, z0 = v0[2];
			barycentric5(v0, v1, v2, x + 0.5, y + 0.5, x0, y0, z0);
			im.at<Vec3b>(height - y - 1, x) = Vec3b(120, 120, 120);
			/*if (x0 < 0 || y0 < 0 || z0 < 0) continue;
			float z = 0;
			z = x0 * v0[2] + y0 * v1[2] + z0 * v2[2];

			if (z >= zbuff(y, x))continue;
			zbuff(y, x) = z;
			
			
			Vec3b color;
			refinebc(x0, y0, z0, z1, z2, z3);
			fragmentShader3(x0, y0, z0, color, difftexture, nmtexture, spectexture, indexs);
			im.at<Vec3b>(height - y - 1, x) = color;*/
			
		}
	}
	return 1;
};
int ComplexShader::vertexShader2(const Vector4f& position0, const Vector2f& uv0,const Vector4f& n0, const Matrix4f& mvp,int t) {

	positions[t].noalias() = mvp * position0;
	
	if (shadow_on)
	{
		shadowpositions[t] = uniform_LVP * uniform_LP * uniform_LV * uniform_M * position0;
		float w = shadowpositions[t][3];
		shadowpositions[t].noalias() = shadowpositions[t] / w;
		//light_pos = vec3(light_po[0], light_po[1], light_po[2]);
	}
	float w = positions[t][3];
	if (w == 0) { positions[t].noalias() = positions[t]; }
	else {
		positions[t].noalias() = positions[t] / w;
	}
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
int ComplexShader::fragmentShader3(float x, float y, float z, Vec3b& color, int difftexture, int nmtexture, int spectexture,int indexs[3]) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 30.f, spec = 16.f;
	Vector4f n0(0.f, 0.f, 0.f, 0.f);
	Vector4f col(255.f, 255.f, 255.f, 0.f);
	//if (uvs[indexs[0]][0] < 0 || uvs[indexs[0]][0]>1 || uvs[indexs[1]][0] < 0 || uvs[indexs[1]][0]>1) { cout << "uvs" << uvs[indexs[0]][0] << uvs[indexs[1]][0] << endl; }
	u = x * fmod(uvs[indexs[0]][0],1.f) + y * fmod(uvs[indexs[1]][0],1.f) + z * fmod(uvs[indexs[2]][0],1.f);
	v = x * fmod(uvs[indexs[0]][1], 1.f) + y * fmod(uvs[indexs[1]][1], 1.f) + z * fmod(uvs[indexs[2]][1], 1.f);
	n0[0] = x*normals[indexs[0]][0] + y * normals[indexs[1]][0] + z * normals[indexs[2]][0];
	n0[1] = x * normals[indexs[0]][1] + y * normals[indexs[1]][1] + z * normals[indexs[2]][1];
	n0[2] = x * normals[indexs[0]][2] + y * normals[indexs[1]][2] + z * normals[indexs[2]][2];
	
	
	if (shadow_on == 1 ) {
		float shadow_posx = x * shadowpositions[indexs[0]][0] + y * shadowpositions[indexs[1]][0] + z * shadowpositions[indexs[2]][0];
		float shadow_posy = x * shadowpositions[indexs[0]][1] + y * shadowpositions[indexs[1]][1] + z * shadowpositions[indexs[2]][1];
		float shadow_posz = x * shadowpositions[indexs[0]][2] + y * shadowpositions[indexs[1]][2] + z * shadowpositions[indexs[2]][2];
		//cout<<"shadowmap.get(shadow_posx, shadow_posy).r:"<< (int)shadowmap.get(shadow_posx, shadow_posy).r <<" shadow_posz:"<< shadow_posz*255 << endl;
		//float bias = std::max(0.01f * (1.0f - abs(n0.dot(light_dirs[0]))), 0.01f);
		float bias = 0.005f;
		if (shadow_posy >=0 && shadow_posy < shadowmap.rows() && shadow_posx>=0 && shadow_posx < shadowmap.cols())
		{		
			if (shadowmap(shadow_posy, shadow_posx ) + bias < shadow_posz) {
			color = Vec3b(ambient,ambient ,ambient);
			//color = Vec3b(col[0], col[1], col[2]);
			return 1;
		}
		}

	}

	if (difftexture >= 0) {
		//cout << "u,v" << u << v << endl;
		if (u < 0 || u>1 || v < 0 || v>1) { cout << "u,v" << u << v << endl; }
		MyColor& coltex = textures[difftexture].get(u * textures[difftexture].get_width(), v * textures[difftexture].get_height());
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}

	if (nmtexture >= 0) {

		MyColor& col_nm = textures[nmtexture].get(u * textures[nmtexture].get_width(), v * textures[nmtexture].get_height());
		//cout << "col_nm" << col_nm.r << endl;
		n0 = Vector4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
	}
	if (spectexture >= 0) {
		MyColor& col_spec = textures[spectexture].get(u * textures[spectexture].get_width(), v * textures[spectexture].get_height());
		spec = col_spec.b / 1.f;
	}
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
int ComplexShader::fragmentShader_shadow(float x, float y, float z, float x_shadow, float y_shadow, float z_shadow, Vec3b& color, int difftexture, int nmtexture, int spectexture, int indexs[3]) {
	float  u = 0.f, v = 0.f, intense_diff = 0.f, intense_spec = 0.f, ambient = 30.f, spec = 16.f;
	Vector4f n0(0.f, 0.f, 0.f, 0.f);
	Vector4f col(255.f, 255.f, 255.f, 0.f);

	u = x * uvs[indexs[0]][0] + y * uvs[indexs[1]][0] + z * uvs[indexs[2]][0];
	v = x * uvs[indexs[0]][1] + y * uvs[indexs[1]][1] + z * uvs[indexs[2]][1];
	n0[0] = x * normals[indexs[0]][0] + y * normals[indexs[1]][0] + z * normals[indexs[2]][0];
	n0[1] = x * normals[indexs[0]][1] + y * normals[indexs[1]][1] + z * normals[indexs[2]][1];
	n0[2] = x * normals[indexs[0]][2] + y * normals[indexs[1]][2] + z * normals[indexs[2]][2];


	if (shadow_on == 1) {
		float shadow_posx = x_shadow * shadowpositions[indexs[0]][0] + y_shadow * shadowpositions[indexs[1]][0] + z_shadow * shadowpositions[indexs[2]][0];
		float shadow_posy = x_shadow * shadowpositions[indexs[0]][1] + y_shadow * shadowpositions[indexs[1]][1] + z_shadow * shadowpositions[indexs[2]][1];
		float shadow_posz = x_shadow * shadowpositions[indexs[0]][2] + y_shadow * shadowpositions[indexs[1]][2] + z_shadow * shadowpositions[indexs[2]][2];
		//cout<<"shadowmap.get(shadow_posx, shadow_posy).r:"<< shadowmap(shadow_posy, shadow_posx) <<" shadow_posz:"<< shadow_posz << endl;
		float bias = std::max(0.01f * (1.0f - abs(n0.dot(light_dirs[0]))), 0.01f);
		
		if (shadowmap(shadow_posy, shadow_posx) + bias < shadow_posz) {
			color = Vec3b(ambient, ambient, ambient);
			//color = Vec3b(col[0], col[1], col[2]);
			return 1;
		}

	}

	if (difftexture >= 0) {
		MyColor& coltex = textures[difftexture].get(u * textures[difftexture].get_width(), v * textures[difftexture].get_height());
		col[0] = coltex.b; col[1] = coltex.g; col[2] = coltex.r;
	}

	if (nmtexture >= 0) {

		MyColor& col_nm = textures[nmtexture].get(u * textures[nmtexture].get_width(), v * textures[nmtexture].get_height());
		cout << "col_nm" << col_nm.r << endl;
		n0 = Vector4f(col_nm.r / 255.f * 2.f - 1.f, col_nm.g / 255.f * 2.f - 1.f, col_nm.b / 255.f * 2.f - 1.f, 0.f);
	}
	if (spectexture >= 0) {
		MyColor& col_spec = textures[spectexture].get(u * textures[spectexture].get_width(), v * textures[spectexture].get_height());
		spec = col_spec.b / 1.f;
	}
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
int ShadowShader::vertexShader(const Vector4f& position0, int t) {
	positions[t].noalias() = uniform_VP * uniform_P * uniform_V * uniform_M * position0;
	
	float w = positions[t][3];
	if (w == 0) { positions[t].noalias() = positions[t]; }
	else {
		positions[t].noalias() = positions[t] / w;
	}
	return 1;
};
int ShadowShader::fragmentShader(Vector3f& bc, Vec3b& color) {
	return 1;

};
int ShadowShader::drawTriagle( const Vector4f& v0, const Vector4f& v1, const Vector4f& v2, MatrixXd& zbuff) {
	/*
	if (vertexs.size() > 3 || vertexs.size() <= 0) {
		cout << "drawTriagle 异常" << endl;
		return -1;
	}*/
	
	int weight = zbuff.cols();
	int height = zbuff.rows();

	//Vector2f bboxmin(weight - 1, height - 1);
	//Vector2f bboxmax(0, 0);
	//Vector2f clamp(weight - 1, height - 1);
	int minx = weight - 1, miny = height - 1, maxx = 0, maxy = 0;
	{
		minx = max(0, min(minx, (int)v0[0])); minx = max(0, min(minx, (int)v1[0])); minx = max(0, min(minx, (int)v2[0]));
		miny = max(0, min(miny, (int)v0[1])); miny = max(0, min(miny, (int)v1[1])); miny = max(0, min(miny, (int)v2[1]));
		maxx = min(weight - 1, max(maxx, (int)v0[0])); maxx = min(weight - 1, max(maxx, (int)v1[0])); maxx = min(weight - 1, max(maxx, (int)v2[0]));
		maxy = min(height - 1, max(maxy, (int)v0[1])); maxy = min(height - 1, max(maxy, (int)v1[1])); maxy = min(height - 1, max(maxy, (int)v2[1]));
	}

	//return (maxx - minx + 1) * (maxy - miny + 1);
	for (int x = minx; x <= maxx; x++) {
		for (int y = miny; y <= maxy; y++) {
			//cout <<"x=" << x<<"y=" << y << endl;
			
			//int sample = (v0[2] + 1) * 2;
			//cout << "z=" << v0[2] << endl;
			float x0 = x, y0 = y, z0 = v0[2];
			barycentric5(v0, v1, v2, x + 0.5, y + 0.5, x0, y0, z0);
			if (x0 < 0 || y0 < 0 || z0 < 0) continue;
			float z = 0.f;
			z = x0 * v0[2] + y0 * v1[2] + z0 * v2[2];
			//cout << "z=" << z << endl;
			if (z >= zbuff(y, x))continue;
			zbuff(y, x) = z; 
			//cout << "zbuff(y, x) =" << zbuff(y, x) << endl;
		}
	}
	return 1;
};
int ShadowShader::drawObject( Model& mymodel, MatrixXd& zbuff) {
	
	for (int i = 0; i < mymodel.meshes.size(); i++) {
		Mesh& mesh = mymodel.meshes[i];
		positions.clear();
		positions.resize(mesh.vertices.size());
		#pragma omp parallel for
		for (int j = 0; j < mesh.vertices.size(); j += 1) {
			vertexShader(mesh.vertices[j].v, j);

		}

		#pragma omp parallel for
		for (int j = 0; j < mesh.indices.size(); j += 3) {
			//cout << indexs[0]<<" " << " "<<indexs[1]<< " " << indexs[2] << endl;
			Vector4f& v0 = positions[mesh.indices[j]];
			Vector4f& v1 = positions[mesh.indices[j + 1]];
			Vector4f& v2 = positions[mesh.indices[j + 2]];
			this->drawTriagle( v0, v1, v2, zbuff);
			
			//fragmentShader(image, v0, v1, v2, *complexshader, zbuff, difftexture, nmtexture, spectexture, indexs);
		}
	}
	return 1;
};