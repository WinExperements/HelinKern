#include <backends/sdl2_backend.h>
#include <SDL/SDL2.h>
class SDLBackend : public Backend {
	public:
		SDLBackend() {}
		bool Init() {
			if (SDL_Init(SDL_INIT_EVERYTHING)) {
				return false;
			}
			this->wnd = SDL_CreateWindow("SDL2 Backend",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,800,640,0);
			if (!this->wnd) {
				return false;
			}
			this->surf = SDL_GetWindowSurface(this->wnd);
			if (!this->surf) {
				SDL_DestroyWindow(this->wnd);
				return false;
			}
			return true;
		}

	private:
		SDL_Window *wnd;
		SDL_Surface *surf;
};

Backend *CreateSDL2Backend() {
	Backend *b = new SDLBackend();
	return b;
}
