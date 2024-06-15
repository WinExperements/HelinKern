/* Software renderer */
#include <software_render.h>
class SoftwareRender : public RenderCore {
	public:
		SoftwareRender() {}
		bool Init(int width,int height,void *buffer) {
			/* I don't remember how to use C++ */
			this->w = width;
			this->h = height;
			this->buffer = (char *)buffer;
			return true;
		}
		void ClearScreen(int r,int g,int b) {
			for (int y = 0; y < this->h; y++) {
				for (int x = 0; x < this->w; x++) {
					int pos = (y * w + x) * 4;
					buffer[pos] = r;
					buffer[pos+1] = g;
					buffer[pos+2] = b;
					buffer[pos+3] = 255;
				}
			}
		}
		void Destroy() {}
	private:
		int w,h;
		char *buffer;
};

RenderCore *CreateSoftwareRender() {
	return new SoftwareRender();
}
