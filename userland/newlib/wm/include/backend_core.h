#pragma once
class Backend {
	public:
		Backend() {}
		bool Init() {return false;}
		int GetWidth() {return 0;}
		int GetHeight() {return 0;}
		void *GetDataBuffer() {return nullptr;}
		// Each backend must process it's own set of events(Maybe)
		void ProcessEvents() {}
		// Check if exit required(for example if the user requested compositor exit or system is killed the process)
		bool IsExitRequired() {return true;}
		void Destroy() {}
		~Backend() {Destroy();}
};
