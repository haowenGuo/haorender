#include"render.h"
#include <omp.h>
shared_ptr<TGAImage>  readTga(const char* path) {
	shared_ptr<TGAImage>image=make_shared<TGAImage>();

	// 读取TGA文件
	if (!image->read_tga_file(path)) {
		std::cerr << "无法读取TGA文件！" << std::endl;
		return make_shared<TGAImage>();
	}
	
	// 垂直翻转图像（如果需要）
	// TGA文件默认是从上到下存储，而大多数图形系统是从下到上
	image->flip_vertically();

	// 获取图像信息
	int width = image->get_width();
	int height = image->get_height();
	int bpp = image->get_bytespp(); // 每像素字节数
	
	return image;
}

int render::add_Light(const Vector4f& l) {
	light_dir.push_back(l.normalized());
	complexshader->addLight(*(light_dir.end() - 1));
	return 1;
}
int render::set_translation(float dx, float dy, float dz) {
	Eigen::Matrix4f translate;
	translate << 1, 0, 0, dx,
		0, 1, 0, dy,
		0, 0, 1, dz,
		0, 0, 0, 1;
	translate;
	model = translate *model;
	return 1;
};
int render::set_scal(float dx, float dy, float dz) {
	Eigen::Matrix4f scal;
	scal << dx, 0, 0, 0,
		     0, dy, 0, 0,
		      0, 0, dz, 0,
		       0, 0, 0, 1;
	     
	model = scal * model;
	return 1;
};

int render::set_rotate(float angle, const Vector3f& axis) {
	angle = angle * MY_PI / 180.0f;
	Vector3f normalizedAxis = axis.normalized();
	float x = normalizedAxis[0];
	float y = normalizedAxis[1];
	float z = normalizedAxis[2];

	// 计算旋转矩阵元素
	float c = cos(angle);
	float s = sin(angle);
	float t = 1 - c;

	// 3x3旋转矩阵部分
	float r00 = t * x * x + c;
	float r01 = t * x * y - s * z;
	float r02 = t * x * z + s * y;
	float r10 = t * x * y + s * z;
	float r11 = t * y * y + c;
	float r12 = t * y * z - s * x;
	float r20 = t * x * z - s * y;
	float r21 = t * y * z + s * x;
	float r22 = t * z * z + c;
	Eigen::Matrix4f rotate;
	rotate << r00, r01, r02, 0,
		r10, r11, r12, 0,
		r20, r21, r22, 0,
		0, 0, 0, 1;
	// 4x4齐次旋转矩阵
	model = rotate*model;
	return 1;
}; 


int render::set_view(Vector3f eye_pos, Vector3f centre, Vector3f up) {

	Eigen::Matrix4f translate;
	translate << 1, 0, 0, -eye_pos[0],
		0, 1, 0, -eye_pos[1],
		0, 0, 1, -eye_pos[2],
		0, 0, 0, 1;
	Vector3f z = eye_pos - centre;
	Vector3f x = up.cross(z);
	Vector3f y = z.cross(x);
	z.normalize();
	x.normalize();
	y.normalize();
	Eigen::Matrix4f view_trans;
	view_trans << x[0], x[1], x[2], 0,
		y[0], y[1], y[2], 0,
		z[0], z[1], z[2], 0,
		0, 0, 0, 1;

	view= view_trans * translate;
	return 1; 
};
Matrix4f render::get_view(Vector3f eye_pos, Vector3f centre, Vector3f up) {
	Eigen::Matrix4f translate;
	translate << 1, 0, 0, -eye_pos[0],
		0, 1, 0, -eye_pos[1],
		0, 0, 1, -eye_pos[2],
		0, 0, 0, 1;
	Vector3f z = eye_pos - centre;
	Vector3f x = up.cross(z);
	Vector3f y = z.cross(x);
	z.normalize();
	x.normalize();
	y.normalize();
	Eigen::Matrix4f view_trans;
	view_trans << x[0], x[1], x[2], 0,
		y[0], y[1], y[2], 0,
		z[0], z[1], z[2], 0,
		0, 0, 0, 1;
	//view = view_trans * translate;
	return view_trans * translate;;
};

int render::set_projection(float eye_fov, float aspect_ratio, float zNear, float zFar) {
	//std::cout << "eye_fov=" << eye_fov << std::endl;
	eye_fov = eye_fov * MY_PI / 180.0f;
	float tanfov = tan(eye_fov / 2.0);
	Eigen::Matrix4f p;
	p << 1 / (aspect_ratio * tanfov), 0, 0, 0,
		0, 1 / (tanfov), 0, 0,
		0, 0, -1 * ((zNear + zFar) / (zFar - zNear)), -1 * ((2 * zNear * zFar) / (zFar - zNear)),
		0, 0, -1, 0;

	projection = p;
	return 1;
};
Matrix4f render::getOrthographic(float left, float right, float bottom, float top, float near, float far) {
	Matrix4f mat = Matrix4f::Identity();

	mat(0, 0) = 2.0f / (right - left);
	mat(1, 1) = 2.0f / (top - bottom);
	mat(2, 2) = -2.0f / (far - near);

	mat(0, 3) = -(right + left) / (right - left);
	mat(1, 3) = -(top + bottom) / (top - bottom);
	mat(2, 3) = -(far + near) / (far - near);

	return mat;
}
int render::set_viewport(float x, float y, float w, float h) {
	Eigen::Matrix4f viewport0 ;
	viewport0 << w / 2.0f, 0, 0, x + w / 2.0f,
		0, h / 2.0f, 0, y + h / 2.0f,
		0, 0, 1, 0,
		0, 0, 0, 1;
	this->viewport = viewport0;
	return 1;
}
Matrix4f render::get_viewport(float x, float y, float w, float h) {
	Eigen::Matrix4f viewport0;
	viewport0 << w / 2.0f, 0, 0, x + w / 2.0f,
		0, h / 2.0f, 0, y + h / 2.0f,
		0, 0, 1, 0,
		0, 0, 0, 1;
	return viewport0;
}
Matrix4f render::get_viewport2(float x, float y, float w, float h,float near, float far) {
	Eigen::Matrix4f viewport0;
	viewport0 << w / 2.0f, 0, 0, x + w / 2.0f,
		0, h / 2.0f, 0, y + h / 2.0f,
		0, 0, (far - near) / 2.0f, (far + near) / 2.0f,
		0, 0, 0, 1;
	return viewport0;
}

int render::clear(Mat& image) {
	image = Mat::Mat(height, weight, CV_8UC3, Scalar(155, 155, 155));
	zbuff = MatrixXd(height, weight );
	zbuff.setConstant(100000000);
	return 1;
}

int render::closeBackCut() { backcut = 0; return 1; };
int render::setTexture(const char* path) { texture = readTga(path); return 1;}
int render::setNMTexture(const char* path) { nmtexture = readTga(path);return 1; }
int render::setSpecTexture(const char* path) { spectexture = readTga(path); return 1;}
int render::setSimpleShader() {
	myshader = make_shared<SimpleShader>(); 
	myshader->setTgatexture(texture);
	myshader->setNMTexture(nmtexture);
	myshader->setSpecTexture(spectexture);
	return 1; 

}
int render::setComplexShader(const Model& m ) {
	complexshader = make_shared<ComplexShader>();
	size_t maxsize = 0;
	int size = 0;
	for (int i = 0; i < m.meshes.size(); i++) {
		maxsize = max(maxsize, m.meshes[i].vertices.size());
		size += m.meshes[i].vertices.size();
	}
	cout << "顶点数=" << size << endl;
	complexshader->positions.resize(maxsize);
	complexshader->uvs.resize(maxsize);
	complexshader->normals.resize(maxsize);
	complexshader->setTexture(m.images);

	return 1;

}
int render::draw_ShadowTexture(MatrixXd& image, Model& mymodel) {
	/**/
	shared_ptr<ShadowShader> sshader=make_shared<ShadowShader>();
	//MatrixXd zbuff = MatrixXd(image.get_height(), image.get_width());
	//zbuff.setOnes();
	for (int i = 0; i < light_dir.size(); i++) {
		sshader->uniform_M=model;
		sshader->uniform_V = get_view(Vector3f(0,0,0), Vector3f(0, 0, 0)+light_dir[i].head<3>(), Vector3f(0, 1, 0));
		sshader->uniform_P = getOrthographic(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
		sshader->uniform_VP = get_viewport2(0, 0, image.cols(), image.rows(), 0.f, 1.f);
		complexshader->uniform_LV = sshader->uniform_V;
		complexshader->uniform_LP = sshader->uniform_P;
		complexshader->uniform_LVP = sshader->uniform_VP;
		sshader->drawObject( mymodel, image);
	}

	return 1;
};
int	render::draw_completed(Mat& image, Model& mymodel) {
	int size = 0;
	if (complexshader == nullptr) { cout<<"没有设置正确的着色器" << endl; return -1; }
	/*
	for (int i = 0; i < light_dir.size(); i++) {
		complexshader->addLight(light_dir[i]);
	}*/
	if (shadow_on) {
		MatrixXd shadow_map= MatrixXd::Ones(image.rows, image.cols);
		//shadow_map.height = image.rows;
		//shadow_map.width = image.cols;
		//ShadowShader sh;
		draw_ShadowTexture(shadow_map, mymodel);
		complexshader->shadowmap = move(shadow_map);
		complexshader->shadow_on = 1;

		/*
		for (int i = 0; i < shadow_map.width; i++)
		{
			for (int t = 0; t < shadow_map.height; t++) {
				unsigned char d = complexshader->shadowmap.get(i,t).r;
				image.at<Vec3b>(t, i) = Vec3b(d, d, d);
			}
		}*/
		//return 1;
	}
	complexshader->setUniform_M(model);
	complexshader->setUniform_V(view);
	complexshader->setUniform_P(projection);
	complexshader->setUniform_MIT(view);
	for (int i = 0; i < complexshader->light_dirs.size(); i++) {
		complexshader->light_dirs[i]=view* light_dir[i];
	}
	Matrix4f  mvp = viewport * projection * view * model;
	//Matrix4f  mvp = viewport;
	auto start = std::chrono::high_resolution_clock::now();
	vector<Vector4f> vers;
	vers.resize(3);

	for (int i = 0; i < mymodel.meshes.size(); i++) {
		Mesh& mesh = mymodel.meshes[i];
		int f = mesh.vertices[mesh.indices[0]].textures.size();
		omp_set_num_threads(std::thread::hardware_concurrency());
		if (shadow_on) { complexshader->shadowpositions.resize(mesh.vertices.size()); }
		#pragma omp parallel for
		for (int j = 0; j < mesh.vertices.size(); j += 1) {
			if (f > 0) {
				complexshader->vertexShader2(mesh.vertices[j].v, mesh.vertices[j].textures[0], mesh.vertices[j].n, mvp, j);
			}
			else {
				complexshader->vertexShader2(mesh.vertices[j].v, Vector2f(0.f,0.f), mesh.vertices[j].n, mvp, j);
			}
		}

		#pragma omp parallel for
		for (int j = 0; j < mesh.indices.size(); j += 3) {
			if (backcut) {

				Vector4f& v0 = complexshader->positions[mesh.indices[j]];
				Vector4f& v1 = complexshader->positions[mesh.indices[j + 1]];
				Vector4f& v2 = complexshader->positions[mesh.indices[j + 2]];

				// 计算面法线（使用叉乘）
				Vector3f edge1 = (v1 - v0).head(3);
				Vector3f edge2 = (v2 - v0).head(3);
				Vector3f faceNormal = edge1.cross(edge2);
				faceNormal.normalize();

				// 如果点积小于等于0，表示面背对相机，剔除
				if (faceNormal[2] <  0) continue;  // 检查法向量z分量
			}
			
			/*if (complexshader->positions[mesh.indices[j]][0] < 0 || complexshader->positions[mesh.indices[j]][0] >= weight
				|| complexshader->positions[mesh.indices[j]][1] < 0 || complexshader->positions[mesh.indices[j]][1] >= height)
			{
				continue;
			}
				else if (complexshader->positions[mesh.indices[j+1]][0] < 0 || complexshader->positions[mesh.indices[j+1]][0] >= weight
					|| complexshader->positions[mesh.indices[j+1]][1] < 0 || complexshader->positions[mesh.indices[j+1]][1] >= height)
				{
					continue;
				}
					else if (complexshader->positions[mesh.indices[j+2]][0] < 0 || complexshader->positions[mesh.indices[j+2]][0] >= weight
						|| complexshader->positions[mesh.indices[j+2]][1] < 0 || complexshader->positions[mesh.indices[j+2]][1] >= height)
					{
						continue;
					}
			
			if (zbuff_open) {
				if (complexshader->positions[mesh.indices[j]][2] > zbuff(complexshader->positions[mesh.indices[j]][1], complexshader->positions[mesh.indices[j]][0])
					|| complexshader->positions[mesh.indices[j + 1]][2] > zbuff(complexshader->positions[mesh.indices[j + 1]][1], complexshader->positions[mesh.indices[j + 1]][0])
					|| complexshader->positions[mesh.indices[j + 2]][2] > zbuff(complexshader->positions[mesh.indices[j + 2]][1], complexshader->positions[mesh.indices[j + 2]][0]))
					continue;  // 深度测试
			}*/
			int difftexture = -1, nmtexture = -1, spectexture = -1;
			for (int p = 0; p < mesh.textures.size(); p++) {
				
				if (mesh.textures[p].type == "texture_diffuse") {
					difftexture = mesh.textures[p].id; 
				}
				else if (mesh.textures[p].type == "texture_specular") {
					spectexture = mesh.textures[p].id;
				}
				else if (mesh.textures[p].type == "normal_specular") {
					nmtexture = mesh.textures[p].id;
				}
			}
			int indexs[3] = { mesh.indices[j] ,mesh.indices[j + 1] ,mesh.indices[j + 2] };
			//cout << indexs[0]<<" " << " "<<indexs[1]<< " " << indexs[2] << endl;
			Vector4f& v0 = complexshader->positions[mesh.indices[j]];
			Vector4f& v1 = complexshader->positions[mesh.indices[j+1]];
			Vector4f& v2 = complexshader->positions[mesh.indices[j+2]];

			//cout << difftexture <<" " << spectexture<< " " << nmtexture << endl;
			float z0 = (view*model*mesh.vertices[indexs[0]].v)[2], z1 = (view * model * mesh.vertices[indexs[1]].v)[2], z2 = (view * model * mesh.vertices[indexs[2]].v)[2];
			//int p = drawTriagle_completed(image, v0, v1, v2, *complexshader, zbuff, difftexture, nmtexture, spectexture, indexs);// 并行区域代码
			complexshader->drawTriagle_completed(image, zbuff, difftexture, nmtexture, spectexture, indexs,z0,z1,z2);// 并行区域代码
			//size += p;
			//s[p]++;
			//cout <<  "size " << size << endl;*/
		}
	}
	//cout << "size =" << size << endl;
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	//std::cout << "顶点变换+法线，纹理总执行时间: " << duration.count() << " 毫秒" << std::endl;
	//std::cout << "法线，纹理总执行时间: " << duration1.count() * mymodel.meshes.size()* t2 << " 毫秒" << std::endl;
	std::cout << "渲染总执行时间: " << duration.count() << " 毫秒" << std::endl;
	return 1;
}
