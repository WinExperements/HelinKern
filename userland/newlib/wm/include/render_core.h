/* Renderer Core */
#pragma once
class RenderCore {
	public:
		RenderCore() {}
		virtual bool Init(int w,int h,void *buffer) {}
		virtual void ClearScreen(int r,int g,int b) {}
		virtual void Destroy() {}
		~RenderCore() {Destroy();}
};
