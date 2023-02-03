// FFI Imports
// Each function imported from the host environment needs to be assigned to a
// global like this and identified by a constant that the resolver in the C/C++
// code will understand.
console.log = vmImport(1);

var global      = 0
const arraySize = 5;

// Helper to demonstrate that we can make an array.
function makeArray()
{
	var a = [];
	for (var i = 0; i < arraySize; ++i)
	{
		global += 2;
		a[i] = global;
	}
	return a;
}

function sayHello()
{
	console.log('Hello, World!');
	var array = makeArray();
	var i     = 0;
	for (i = 0; i < arraySize; ++i)
	{
		console.log("array[", i, "] = ", array[i]);
	}
}

// FFI exports.  Each function that we export needs to be assigned a unique
// number that can be used to look it up.
vmExport(1234, sayHello);
