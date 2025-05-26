#include"model.h"
vec4::vec4(vec3& v3) { x = v3.x; y = v3.y; z = v3.z; w = 1; }
Mat get_view_matrix(Vec3f eye_pos,Vec3f centre,Vec3f up)
{
	
	Mat translate = (Mat_<float>(4, 4) <<
		1, 0, 0, -eye_pos[0],
		0, 1, 0, -eye_pos[1],
		0, 0, 1,-eye_pos[2],
		0, 0, 0, 1);
	Vec3f z= eye_pos- centre;
	Vec3f x= up.cross(z);
	Vec3f y= z.cross(x);
	normalize(z, z);
	normalize(x, x);
	normalize(y, y);
	Mat view_trans = (Mat_<float>(4, 4) <<
		x[0], x[1], x[2], 0,
		y[0], y[1], y[2], 0,
		z[0], z[1], z[2], 0,
		0, 0, 0, 1);

	return view_trans* translate;
}
Mat get_model_matrix(float dx,float dy,float dz)
{
	Mat translate = (Mat_<float>(4, 4) <<
		1, 0, 0, dx,
		0, 1, 0, dy,
		0, 0, 1, dz,
		0, 0, 0, 1);
	return translate;
}
Mat get_model_matrix(float angle,const Vec3f &axis)
{
	angle = angle * MY_PI / 180.0f;
	Vec3d normalizedAxis = axis / norm(axis);
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

	// 4x4齐次旋转矩阵
	return (Mat_<float>(4, 4) <<
		r00, r01, r02, 0,
		r10, r11, r12, 0,
		r20, r21, r22, 0,
		0, 0, 0, 1);

}

Mat get_projection_matrix(float eye_fov, float aspect_ratio, float zNear, float zFar)
{
	eye_fov = eye_fov * MY_PI / 180.0f;
	float tanfov = tan(eye_fov / 2.0);
	Mat projection = (Mat_<float>(4, 4) <<
		1 / (aspect_ratio * tanfov), 0, 0, 0,
		0, 1 / (tanfov), 0, 0,
		0, 0, -(zNear + zFar) / (zFar - zNear), ( - 2 * zNear * zFar) / (zFar - zNear),
		0, 0, -1, 0);
	return projection;
}
Model modelread(const string& filepath) {
	// 创建导入器
	Assimp::Importer importer;

	// 导入模型（带处理标志）
	const aiScene* scene = importer.ReadFile(
		filepath,
		aiProcess_Triangulate |         // 将多边形分解为三角形
		aiProcess_GenSmoothNormals |    // 生成平滑法线
		aiProcess_JoinIdenticalVertices // 合并重复顶点
	);
	//aiProcess_FlipUVs | // 翻转UV坐标（DirectX风格）
	// 检查导入是否成功
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "导入错误: " << importer.GetErrorString() << std::endl;
		return Model();
	}
	Model model;
	model.processNode(scene->mRootNode, scene);
	return model;
}
int Model::modelread(const string& filepath) {
	// 创建导入器
	Assimp::Importer importer;

	// 导入模型（带处理标志）
	const aiScene* scene = importer.ReadFile(
		filepath,
		aiProcess_Triangulate |         // 将多边形分解为三角形
		aiProcess_GenSmoothNormals |    // 生成平滑法线
		aiProcess_JoinIdenticalVertices // 合并重复顶点
	);
	//aiProcess_FlipUVs | // 翻转UV坐标（DirectX风格）
	// 检查导入是否成功
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "导入错误: " << importer.GetErrorString() << std::endl;
		return -1;
	}
	this->directory = filepath.substr(0, filepath.find_last_of('/'));
	this->processNode(scene->mRootNode, scene);
	return 1;
}
int Model::processNode(aiNode* node, const aiScene* scene) {
	// process all the node's meshes (if any)
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene));	
	}
	// then do the same for each of its children
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene);
	}
	return 1;
}
int Model::TextureFromFile(const char* path, const string directory) {
	string imagePath = string(path);
	imagePath = directory + '/' + imagePath;
	int width, height, channels;

	// 加载图像
	unsigned char* imageData = stbi_load(
		imagePath.c_str(),  // 图像路径
		&width,             // 宽度指针
		&height,            // 高度指针
		&channels,          // 通道数指针
		0                   // 强制通道数(0表示使用原始通道数)
	);

	// 检查图像是否成功加载
	if (imageData == nullptr) {
		std::cerr << "无法加载图像: " << imagePath << std::endl;
		std::cerr << "错误: " << stbi_failure_reason() << std::endl;
		return 1;
	}

	// 输出图像信息
	std::cout << "成功加载图像: " << imagePath << std::endl;
	std::cout << "宽度: " << width << " 像素" << std::endl;
	std::cout << "高度: " << height << " 像素" << std::endl;
	std::cout << "通道数: " << channels << std::endl;
	MyImage ima(imageData, width, height, channels);
	//images.push_back(imageData, width, height, channels) ;
	images.push_back(move(ima)) ;
	stbi_image_free(imageData);
	// 释放图像数据
	return images.size()-1;
}
vector<texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName)
{
	vector<texture> textures;
	for (int i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);
		texture texture0;
		texture0.id = TextureFromFile(str.C_Str(), this->directory);
		cout << "id=" << texture0.id << endl;
		texture0.type = typeName;
		//texture0.path = str;
		textures.push_back(texture0);
	}
	return textures;
}
Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
	vector<vertex> vertices;
	vector<unsigned int> indices;
	vector<texture> textures;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		vertex vertex;
		// process vertex positions, normals and texture coordinates
		
		vertex.v[0] = mesh->mVertices[i].x;
		vertex.v[1] = mesh->mVertices[i].y;
		vertex.v[2] = mesh->mVertices[i].z;
		vertex.v[3] = 1.f;

		vertex.n[0] = mesh->mNormals[i].x;
		vertex.n[1] = mesh->mNormals[i].y;
		vertex.n[2] = mesh->mNormals[i].z;
		vertex.n[3] = 0.f;
		if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
		{
			
			Vector2f vec;
			vec[0] = mesh->mTextureCoords[0][i].x;
			vec[1] = mesh->mTextureCoords[0][i].y;
			vertex.textures.push_back(vec);
		}
		
		vertices.push_back(vertex);
		
	}

	// process indices
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		vector<texture> diffuseMaps = this->loadMaterialTextures(material,
			aiTextureType_DIFFUSE, "texture_diffuse");
		textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
		vector<texture> specularMaps = this->loadMaterialTextures(material,
			aiTextureType_SPECULAR, "texture_specular");
		textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
	}
	return Mesh(vertices, indices, textures);
}

