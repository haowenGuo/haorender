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
	if (norm(axis) <= 1e-8f) {
		return Mat::eye(4, 4, CV_32F);
	}
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
		aiProcess_CalcTangentSpace |
		aiProcess_JoinIdenticalVertices | // 合并重复顶点
		aiProcess_FixInfacingNormals
	);
	//aiProcess_FlipUVs | // 翻转UV坐标（DirectX风格）
	// 检查导入是否成功
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "导入错误: " << importer.GetErrorString() << std::endl;
		return Model();
	}
	Model model;
	size_t sep = filepath.find_last_of("/\\");
	model.directory = sep == string::npos ? "." : filepath.substr(0, sep);
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
		aiProcess_FlipUVs |
		aiProcess_CalcTangentSpace |
		aiProcess_JoinIdenticalVertices | // 合并重复顶点
		aiProcess_FixInfacingNormals
	);
	//aiProcess_FlipUVs | // 翻转UV坐标（DirectX风格）
	// 检查导入是否成功
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "导入错误: " << importer.GetErrorString() << std::endl;
		return -1;
	}
	size_t sep = filepath.find_last_of("/\\");
	this->directory = sep == string::npos ? "." : filepath.substr(0, sep);
	this->processNode(scene->mRootNode, scene);
	return 1;
}
int Model::processNode(aiNode* node, const aiScene* scene) {
	// process all the node's meshes (if any)
	int p = 0;
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		//cout << "11111"<<p++<<" " << node->mMeshes[i] << endl;
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
	namespace fs = std::filesystem;
	fs::path imagePath = fs::path(path);
	bool isAbsolute = imagePath.is_absolute();
	if (!isAbsolute) {
		imagePath = fs::path(directory) / imagePath;
	}
	string imagePathKey = imagePath.lexically_normal().generic_string();
	auto cacheIt = texture_cache.find(imagePathKey);
	if (cacheIt != texture_cache.end()) {
		return cacheIt->second;
	}
	int width, height, channels;

	// 加载图像
	unsigned char* imageData = stbi_load(
		imagePathKey.c_str(),  // 图像路径
		&width,             // 宽度指针
		&height,            // 高度指针
		&channels,          // 通道数指针
		0                   // 强制通道数(0表示使用原始通道数)
	);

	// 检查图像是否成功加载
	if (imageData == nullptr) {
		std::cerr << "无法加载图像: " << imagePathKey << std::endl;
		std::cerr << "错误: " << stbi_failure_reason() << std::endl;
		return -1;
	}

	// 输出图像信息
	std::cout << "成功加载图像: " << imagePathKey << std::endl;
	std::cout << "宽度: " << width << " 像素" << std::endl;
	std::cout << "高度: " << height << " 像素" << std::endl;
	std::cout << "通道数: " << channels << std::endl;
	MyImage ima(imageData, width, height, channels);
	//images.push_back(imageData, width, height, channels) ;
	images.push_back(move(ima)) ;
	stbi_image_free(imageData);
	// 释放图像数据
	int textureId = static_cast<int>(images.size()) - 1;
	texture_cache[imagePathKey] = textureId;
	return textureId;
}
vector<texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName)
{
	namespace fs = std::filesystem;
	vector<texture> textures;
	for (int i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);
		texture texture0;
		texture0.id = TextureFromFile(str.C_Str(), this->directory);
		if (texture0.id < 0) {
			continue;
		}
		cout << "[Material] " << typeName << " -> " << str.C_Str() << " (id=" << texture0.id << ")" << endl;
		//cout << "id=" << texture0.id << endl;
		texture0.type = typeName;
		texture0.path = fs::path(str.C_Str()).lexically_normal().generic_string();
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
		maxx = max(maxx, vertex.v[0]);
		maxy = max(maxy, vertex.v[1]);
		maxz = max(maxz, vertex.v[2]);
		minx = min(minx, vertex.v[0]);
		miny = min(miny, vertex.v[1]);
		minz = min(minz, vertex.v[2]);
		if (mesh->HasNormals()) {
			vertex.n[0] = mesh->mNormals[i].x;
			vertex.n[1] = mesh->mNormals[i].y;
			vertex.n[2] = mesh->mNormals[i].z;
		}
		else {
			vertex.n[0] = 0.f;
			vertex.n[1] = 0.f;
			vertex.n[2] = 1.f;
		}
		vertex.n[3] = 0.f;
		if (mesh->HasTangentsAndBitangents()) {
			vertex.t[0] = mesh->mTangents[i].x;
			vertex.t[1] = mesh->mTangents[i].y;
			vertex.t[2] = mesh->mTangents[i].z;
			vertex.t[3] = 0.f;
			vertex.b[0] = mesh->mBitangents[i].x;
			vertex.b[1] = mesh->mBitangents[i].y;
			vertex.b[2] = mesh->mBitangents[i].z;
			vertex.b[3] = 0.f;
		}
		else {
			vertex.t = Vector4f(0.f, 0.f, 0.f, 0.f);
			vertex.b = Vector4f(0.f, 0.f, 0.f, 0.f);
		}
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
		vector<texture> baseColorMaps = this->loadMaterialTextures(material,
			aiTextureType_BASE_COLOR, "texture_basecolor");
		textures.insert(textures.end(), baseColorMaps.begin(), baseColorMaps.end());
		vector<texture> normalMaps = this->loadMaterialTextures(material,
			aiTextureType_NORMALS, "texture_normals");
		textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
		vector<texture> heightMaps = this->loadMaterialTextures(material,
			aiTextureType_HEIGHT, "texture_normals");
		textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
		vector<texture> specularMaps = this->loadMaterialTextures(material,
			aiTextureType_SPECULAR, "texture_specular");
		textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		vector<texture> metallicMaps = this->loadMaterialTextures(material,
			aiTextureType_METALNESS, "texture_metallic");
		textures.insert(textures.end(), metallicMaps.begin(), metallicMaps.end());
		vector<texture> roughnessMaps = this->loadMaterialTextures(material,
			aiTextureType_DIFFUSE_ROUGHNESS, "texture_roughness");
		textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());
		vector<texture> aoMaps = this->loadMaterialTextures(material,
			aiTextureType_AMBIENT_OCCLUSION, "texture_ao");
		textures.insert(textures.end(), aoMaps.begin(), aoMaps.end());
		if (aoMaps.empty() && !metallicMaps.empty() && !roughnessMaps.empty()) {
			for (const auto& metallicMap : metallicMaps) {
				for (const auto& roughnessMap : roughnessMaps) {
					if (!metallicMap.path.empty() && metallicMap.path == roughnessMap.path) {
						texture packedAo = metallicMap;
						packedAo.type = "texture_ao";
						textures.push_back(packedAo);
						cout << "[Material] texture_ao -> " << packedAo.path << " (packed ORM fallback, id=" << packedAo.id << ")" << endl;
						goto packed_ao_done;
					}
				}
			}
		}
packed_ao_done:
		vector<texture> emissiveMaps = this->loadMaterialTextures(material,
			aiTextureType_EMISSIVE, "texture_emissive");
		textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
	}
	int countBaseColor = 0;
	int countMetallic = 0;
	int countRoughness = 0;
	int countMetallicRoughness = 0;
	int countAo = 0;
	int countEmissive = 0;
	for (const auto& tex : textures) {
		if (tex.type == "texture_basecolor") {
			++countBaseColor;
		}
		else if (tex.type == "texture_metallic") {
			++countMetallic;
		}
		else if (tex.type == "texture_roughness") {
			++countRoughness;
		}
		else if (tex.type == "texture_metallicroughness") {
			++countMetallicRoughness;
		}
		else if (tex.type == "texture_ao") {
			++countAo;
		}
		else if (tex.type == "texture_emissive") {
			++countEmissive;
		}
	}
	cout << "[Material] PBR Summary: baseColor=" << countBaseColor
		<< " metallic=" << countMetallic
		<< " roughness=" << countRoughness
		<< " metallicRoughness=" << countMetallicRoughness
		<< " ao=" << countAo
		<< " emissive=" << countEmissive << endl;
	return Mesh(vertices, indices, textures);
}
