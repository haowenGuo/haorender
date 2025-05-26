#include"drawer.h"

int drawLine(Mat& im, int weight, int height, const vector<vector<int>>& vers) {
	if (vers.size() > 2 || vers.size() <= 0) {
		cout << "drawLine 异常" << endl;
		return -1;
	}
	vector<int> v1 = vers[0];
	vector<int> v2 = vers[1];
	Vec3b pixel(255, 255, 255);
	float dx = v2[0] - v1[0];
	float dy = v2[1] - v1[1];
	if (abs(dx) > abs(dy)) {
		if (v1[0] > v2[0]) {
			vector<int> temp = v1;
			v1 = v2;
			v2 = temp;
		}
		float k = (float)(v2[1] - v1[1]) / (float)(v2[0] - v1[0]);
		for (int x = v1[0]; x < v2[0]; x++) {
			int y = round((x - v1[0]) * k + v1[1]);
			im.at<Vec3b>(y, x) = pixel;
		}
	}
	else {
		if (v1[1] > v2[1]) {
			vector<int> temp = v1;
			v1 = v2;
			v2 = temp;
		}
		float k = (float)(v2[0] - v1[0]) / (float)(v2[1] - v1[1]);

		for (int y = v1[1]; y < v2[1]; y++) {
			int x = round((y - v1[1]) * k + v1[0]);
			im.at<Vec3b>(y, x) = pixel;
		}
	}
}
int drawLine(Mat& im, int weight, int height, const vector<vector<int>>& vers, const Vec3b &color) {
	if (vers.size() > 2 || vers.size() <= 0) {
		cout << "drawLine 异常" << endl;
		return -1;
	}
	vector<int> v1 = vers[0];
	vector<int> v2 = vers[1];
	const Vec3b &pixel=color;
	float dx = v2[0] - v1[0];
	float dy = v2[1] - v1[1];
	if (abs(dx) > abs(dy)) {
		if (v1[0] > v2[0]) {
			vector<int> temp = v1;
			v1 = v2;
			v2 = temp;
		}
		float k = (float)(v2[1] - v1[1]) / (float)(v2[0] - v1[0]);
		for (int x = v1[0]; x < v2[0]; x++) {
			int y = round((x - v1[0]) * k + v1[1]);
			if(y<height && y>=0 && x<weight && x>=0){ im.at<Vec3b>(y, x) = pixel; }
			
		}
	}
	else {
		if (v1[1] > v2[1]) {
			vector<int> temp = v1;
			v1 = v2;
			v2 = temp;
		}
		float k = (float)(v2[0] - v1[0]) / (float)(v2[1] - v1[1]);

		for (int y = v1[1]; y < v2[1]; y++) {
			int x = round((y - v1[1]) * k + v1[0]);
			im.at<Vec3b>(y, x) = pixel;
		}
	}
}

int drawTriagle(Mat& im, int weight, int height, const vector<vector<int>>& ves) {
	if (ves.size() > 3 || ves.size() <= 0) {
		cout << "drawTriagle 异常" << endl;
		return -1;
	}
	vector<vector<int>> vers;
	vector<int> a = { ves[0][0],ves[0][1] };
	vector<int> b = { ves[1][0],ves[1][1] };
	vector<int> c = { ves[2][0],ves[2][1] };
	vers.emplace_back(a);
	vers.emplace_back(b);
	drawLine(im, weight, height, vers);
	vers.clear();
	vers.emplace_back(b);
	vers.emplace_back(c);
	drawLine(im, weight, height, vers);
	vers.clear();
	vers.emplace_back(c);
	vers.emplace_back(a);
	drawLine(im, weight, height, vers);
	return 1;
}
int drawTriagleProfile(Mat& im, int weight, int height, const vector<vector<int>>& ves, const Vec3b& color) {
	if (ves.size() > 3 || ves.size() <= 0) {
		cout << "drawTriagle 异常" << endl;
		return -1;
	}
	vector<vector<int>> vers;
	vector<int> a = { ves[0][0],ves[0][1] };
	vector<int> b = { ves[1][0],ves[1][1] };
	vector<int> c = { ves[2][0],ves[2][1] };
	vers.emplace_back(a);
	vers.emplace_back(b);
	drawLine(im, weight, height, vers, color);
	vers.clear();
	vers.emplace_back(b);
	vers.emplace_back(c);
	drawLine(im, weight, height, vers, color);
	vers.clear();
	vers.emplace_back(c);
	vers.emplace_back(a);
	drawLine(im, weight, height, vers, color);
	return 1;
}

int drawTriagle(Mat& im, int weight, int height, const vector<vector<int>>& ves, const Vec3b& color) {
	if (ves.size() > 3 || ves.size() <= 0) {
		cout << "drawTriagle 异常" << endl;
		return -1;
	}
	//drawTriagleProfile(im, weight, height, ves,  color);
	vector<vector<int>> vers;
	vector<int> a = { ves[0][0],ves[0][1] };
	vector<int> b = { ves[1][0],ves[1][1] };
	vector<int> c = { ves[2][0],ves[2][1] };
	if (a[1] > b[1]) { vector<int> temp = a; a = b; b = temp; }
	if (b[1] > c[1]) { vector<int> temp = b; b = c; c = temp; }
	if (a[1] > b[1]) { vector<int> temp = a; a = b; b = temp; }
	for (int y = a[1]; y < c[1]; y++) {
		float t = (float)(y - a[1]) / (float)(c[1] - a[1]);
		int x1 = 0;
		int x2 = t * c[0] + (1 - t) * a[0];
		if (y < b[1]) {

			float t2 = (float)(y - a[1]) / (float)(b[1] - a[1]);
			x1= (t2 * b[0] + (1 - t2) * a[0]);
		}
		else {
			float t2 = (float)(y - b[1]) / (float)(c[1] - b[1]);
			x1 = (t2 * c[0] + (1 - t2) * b[0]);
		}
		vector<int> v0 = { x1,y };
		vector<int> v1 = { x2,y };
		vers.clear();
		vers.emplace_back(v0);
		vers.emplace_back(v1);
		drawLine(im, weight, height, vers, color);
	}

	return 1;
}

Vec3f barycentric(const vector<vector<int>>& pts, Vec2i P) {
	Vec3f u = Vec3f(pts[2][0] - pts[0][0], pts[1][0] - pts[0][0], pts[0][0] - P[0]).cross(Vec3f(pts[2][1] - pts[0][1], pts[1][1] - pts[0][1], pts[0][1] - P[1])) ;
	/* `pts` and `P` has integer value as coordinates
	   so `abs(u[2])` < 1 means `u[2]` is 0, that means
	   triangle is degenerate, in this case return something with negative coordinates */
	if (std::abs(u[2]) < 1) return Vec3f(-1, 1, 1);
	return Vec3f(1.f - (u[0] + u[1]) / u[2], u[1] / u[2], u[0] / u[2]);
}
Vec3f barycentric2(const vector<vector<float>>& pts, Vec2i P) {
	Vec3f u = Vec3f(pts[2][0] - pts[0][0], pts[1][0] - pts[0][0], pts[0][0] - P[0]).cross(Vec3f(pts[2][1] - pts[0][1], pts[1][1] - pts[0][1], pts[0][1] - P[1]));
	/* `pts` and `P` has integer value as coordinates
	   so `abs(u[2])` < 1 means `u[2]` is 0, that means
	   triangle is degenerate, in this case return something with negative coordinates */
	if (std::abs(u[2]) < 1) return Vec3f(-1, 1, 1);
	return Vec3f(1.f - (u[0] + u[1]) / u[2], u[1] / u[2], u[0] / u[2]);
}
Vec3f barycentric3(const vector<vec4>& pts, Vec2i P) {
	Vec3f u = Vec3f(pts[2].x - pts[0].x, pts[1].x - pts[0].x, pts[0].x - P[0]).cross(Vec3f(pts[2].y - pts[0].y, pts[1].y - pts[0].y, pts[0].y - P[1]));
	/* `pts` and `P` has integer value as coordinates
	   so `abs(u[2])` < 1 means `u[2]` is 0, that means
	   triangle is degenerate, in this case return something with negative coordinates */
	if (std::abs(u[2]) < 1) return Vec3f(-1, 1, 1);
	return Vec3f(1.f - (u[0] + u[1]) / u[2], u[1] / u[2], u[0] / u[2]);
}
Vector3f barycentric4(const vector<Vector4f>& pts, Vector2i P) {
	Vector3f u = Vector3f(pts[2][0] - pts[0][0], pts[1][0] - pts[0][0], pts[0][0] - P[0]).cross(Vector3f(pts[2][1] - pts[0][1], pts[1][1] - pts[0][1], pts[0][1] - P[1]));
	/* `pts` and `P` has integer value as coordinates
	   so `abs(u[2])` < 1 means `u[2]` is 0, that means
	   triangle is degenerate, in this case return something with negative coordinates */
	if (std::abs(u[2]) < 1) return Vector3f(-1, 1, 1);
	return Vector3f(1.f - (u[0] + u[1]) / u[2], u[1] / u[2], u[0] / u[2]);
}
void barycentric5(const Vector4f& pts0, const Vector4f& pts1, const Vector4f& pts2, int x,int y,int &x0, int& y0, int& z0) {
	/*float s1x = pts2[0] - pts0[0];
	float s1y = pts2[1] - pts0[1];
	float s2x = pts1[0] - pts0[0];
	float s2y = pts1[1] - pts0[1];
	float tx = pts0[0] - x;
	float ty = pts0[1] - y;

	// 手动展开叉乘计算
	float u0 = s1y * tx - s1x * ty;
	float u1 = s2x * ty - s2y * tx;
	float u2 = s1x * s2y - s1y * s2x;

	if (std::abs(u2) < 1) {
		x0 = -1; y0 = 1; z0 = 1;
		return;
	}

	// 预计算倒数，将除法转为乘法
	float inv_u2 = 1.0f / u2;
	x0 = 1.0f - (u0 + u1) * inv_u2;
	y0 = u1 * inv_u2;
	z0 = u0 * inv_u2;*/
	
	Vector3f u = Vector3f(pts2[0] - pts0[0], pts1[0] - pts0[0], pts0[0] - x).cross(Vector3f(pts2[1] - pts0[1], pts1[1] - pts0[1], pts0[1] - y));

	if (std::abs(u[2]) < 1) { x0 = -1; y0 = 1; z0 = 1; //return Vector3f(-1, 1, 1) 
	};
	x0 = 1.f - (u[0] + u[1]) / u[2]; 
	y0 = u[1] / u[2];
	z0 = u[0] / u[2];
	//return Vector3f(1.f - (u[0] + u[1]) / u[2], u[1] / u[2], u[0] / u[2]);
}

int drawTriagle2(Mat& im, int weight, int height, const vector<vector<int>>& ves, const Vec3b& color, Mat& zbuff) {
	if (ves.size() > 3 || ves.size() <= 0) {
		cout << "drawTriagle 异常" << endl;
		return -1;
	}

	Vec2i bboxmin(weight - 1, height - 1);
	Vec2i bboxmax(0, 0);
	Vec2i clamp(weight - 1, height - 1);
	for (int i = 0; i < 3; i++) {
		bboxmin[0] = max(0, min(bboxmin[0], ves[i][0]));
		bboxmin[1] = max(0, min(bboxmin[1], ves[i][1]));

		bboxmax[0] = min(clamp[0], max(bboxmax[0], ves[i][0]));
		bboxmax[1] = min(clamp[1], max(bboxmax[1], ves[i][1]));
	}
	Vec2i P;
	for (P[0] = bboxmin[0]; P[0] <= bboxmax[0]; P[0]++) {
		for (P[1] = bboxmin[1]; P[1] <= bboxmax[1]; P[1]++) {
			Vec3f bc_screen = barycentric(ves, P);
			if (bc_screen[0] < 0 || bc_screen[1] < 0 || bc_screen[2] < 0) continue;
			float z = bc_screen[0] * ves[0][2];
			z += bc_screen[1] * ves[1][2];
			z += bc_screen[2] * ves[2][2];
			if (z <= zbuff.at<float>(P[1], P[0]))continue;
			zbuff.at<float>(P[1], P[0]) = z;
			im.at<Vec3b>(P[1], P[0]) = color;
			//image.set(P[0], P[1], color);
		}
	}
	return 1;
}


int drawTriagle_shader(Mat& im,const vector<vec4>& vertexs, SimpleShader& shader, Mat& zbuff) {

	if (vertexs.size() > 3 || vertexs.size() <= 0) {
		cout << "drawTriagle 异常" << endl;
		return -1;
	}
	int weight = im.cols;
	int height = im.rows;
	Vec2f bboxmin(weight - 1, height - 1);
	Vec2f bboxmax(0, 0);
	Vec2f clamp(weight - 1, height - 1);

	for (int i = 0; i < 3; i++) {
		bboxmin[0] = max(.0f, min(bboxmin[0], vertexs[i].x));
		bboxmin[1] = max(.0f, min(bboxmin[1], vertexs[i].y));

		bboxmax[0] = min(clamp[0], max(bboxmax[0], vertexs[i].x));
		bboxmax[1] = min(clamp[1], max(bboxmax[1], vertexs[i].y));
	}
	Vec2i P;
	for (P[0] = bboxmin[0]; P[0] <= bboxmax[0]; P[0]++) {
		for (P[1] = bboxmin[1]; P[1] <= bboxmax[1]; P[1]++) {
			Vec3f bc_screen = barycentric3(vertexs, P);
			if (bc_screen[0] < 0 || bc_screen[1] < 0 || bc_screen[2] < 0) continue;
			float z = 0, u = 0, v = 0;
			for (int i = 0; i < 3; i++) {
				z += bc_screen[i] * vertexs[i].z;
			}

			if (z >= zbuff.at<float>(P[1], P[0]))continue;
			//if (intense <= 0)continue;
			zbuff.at<float>(P[1], P[0]) = z;
			TGAColor col ;
			Vec3b color;
			shader.fragmentShader(bc_screen, color);
			//TGAColor col = texture.get(u * texture.get_width(), v * texture.get_height());
			//Vec3b color(col.b * intense, col.g * intense, col.r * intense);
			im.at<Vec3b>(height-P[1]-1, P[0]) = color;
			//image.set(P[0], P[1], color);
		}
	}
	return 1;
};

int drawTriagle_completed(Mat& im, const Vector4f& v0, const Vector4f& v1, const Vector4f& v2, ComplexShader& shader, MatrixXd& zbuff, int difftexture, int nmtexture, int spectexture,int indexs[3]) {
	/*
	if (vertexs.size() > 3 || vertexs.size() <= 0) {
		cout << "drawTriagle 异常" << endl;
		return -1;
	}*/
	int weight = im.cols;
	int height = im.rows;
	int t = 0;
	
	//Vector2f bboxmin(weight - 1, height - 1);
	//Vector2f bboxmax(0, 0);
	//Vector2f clamp(weight - 1, height - 1);
	int minx= weight - 1, miny= height - 1, maxx=0, maxy=0;
	{
		minx = max(0, min(minx, (int)v0[0])); minx = max(0, min(minx, (int)v1[0])); minx = max(0, min(minx, (int)v2[0]));
		miny = max(0, min(miny, (int)v0[1])); miny = max(0, min(miny, (int)v1[1])); miny = max(0, min(miny, (int)v2[1]));
		maxx = min(weight - 1, max(maxx, (int)v0[0])); maxx = min(weight - 1, max(maxx, (int)v1[0])); maxx = min(weight - 1, max(maxx, (int)v2[0]));
		maxy = min(height - 1, max(maxy, (int)v0[1])); maxy = min(height - 1, max(maxy, (int)v1[1])); maxy = min(height - 1, max(maxy, (int)v2[1]));
	}
	//return (maxx - minx + 1) * (maxy - miny + 1);
	for (int x = minx; x <= maxx; x++) {
		for (int y = miny; y <= maxy; y++) {
			int x0,  y0, z0;
			//for (int t = 0; t < 2011; t++);
			barycentric5(v0, v1, v2, x, y, x0, y0, z0);
			//cout << x0 << ' ' << y0 << ' ' << z0 << endl;
			if (x0 < 0 || y0 < 0 || z0 < 0) continue;
			float z = 0;
			//z += 5.f * 2.f; z += 5.f * 2.f; z += 5.f * 2.f;
			z = x0 * v0[2] + y0 * v1[2] + z0 * v2[2];
			if (z >= zbuff(y, x))continue;
			zbuff(y, x) = z;
			Vec3b color;
			shader.fragmentShader3(x0, y0, z0, color, difftexture, nmtexture, spectexture,indexs);
			im.at<Vec3b>(height - y - 1, x) = color;
			//Vector3f bc_screen = barycentric5(v0, v1, v2, x, y,  x0,  y0,  z0);
			//Vector3f bc_screen = {0.f,0.f, 0.f};
			/*if (bc_screen[0] < 0 || bc_screen[1] < 0 || bc_screen[2] < 0) continue;

			float z = 0, u = 0, v = 0;
			for (int i = 0; i < 3; i++) {
				z += bc_screen[i] * vertexs[i][2];
			}
			if (z >= zbuff(P[1], P[0]))continue;
			//if (intense <= 0)continue;
			zbuff(P[1], P[0]) = z;
			Vec3b color;
			shader.fragmentShader2(bc_screen, color, difftexture, nmtexture, spectexture);
			//TGAColor col = texture.get(u * texture.get_width(), v * texture.get_height());
			//Vec3b color(col.b * intense, col.g * intense, col.r * intense);
			im.at<Vec3b>(height - P[1] - 1, P[0]) = color;
			//image.set(P[0], P[1], color);*/
		}
	}
	/*for (int i = 0; i < 3; i++) {
		bboxmin[0] = max(.0f, min(bboxmin[0], vertexs[i][0]));
		bboxmin[1] = max(.0f, min(bboxmin[1], vertexs[i][1]));

		bboxmax[0] = min(clamp[0], max(bboxmax[0], vertexs[i][0]));
		bboxmax[1] = min(clamp[1], max(bboxmax[1], vertexs[i][1]));
	}
	return (bboxmax[0] - bboxmin[0] + 1) * (bboxmax[1] - bboxmin[1] + 1);*/
	//if(bboxmax[0]< bboxmin[0] || bboxmax[1]< bboxmin[1]){ return -1; }
	Vector2i P;
	/*
	for (P[0] = bboxmin[0]; P[0] <= bboxmax[0]; P[0]++) {
		for (P[1] = bboxmin[1]; P[1] <= bboxmax[1]; P[1]++) {
			
		
			Vector3f bc_screen = barycentric4(vertexs, P);
			//Vector3f bc_screen = {0.f,0.f, 0.f};
			if (bc_screen[0] < 0 || bc_screen[1] < 0 || bc_screen[2] < 0) continue;
			
			float z = 0, u = 0, v = 0;
			for (int i = 0; i < 3; i++) {
				z += bc_screen[i] * vertexs[i][2];
			}
			if (z >= zbuff(P[1], P[0]))continue;
			//if (intense <= 0)continue;
			zbuff(P[1], P[0]) = z;
			Vec3b color;
			shader.fragmentShader2(bc_screen, color, difftexture,  nmtexture,spectexture);
			//TGAColor col = texture.get(u * texture.get_width(), v * texture.get_height());
			//Vec3b color(col.b * intense, col.g * intense, col.r * intense);
			im.at<Vec3b>(height - P[1] - 1, P[0]) = color;
			//image.set(P[0], P[1], color);
		}
	}*/
	return 1;
};

class Drawer {
public:
	Drawer() {

	}
	void drawLine(Mat& image, int width, int height) {

	}

};
