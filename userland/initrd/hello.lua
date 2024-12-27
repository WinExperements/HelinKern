-- This is a test lua script to test the HelinOS port.
function Test()
	print("Hello world from function!");
	print("Trying to open the Makefile file");
	file = io.open("Makefile","r");
	-- Check if file exists.
	if file == nil then
		print("Failed to open file");
		os.exit();
	end
	print(file);
	print(file.read(file));
	io.close();
end
Test();
