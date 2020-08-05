$(function(){
	document.addEventListener("keydown",keyDownHandler, false);
	console.log("Ready!")
});

//-------------
// Key Handlers
//-------------
function keyDownHandler(event)
{
	var keyCode = event.keyCode
	var keyPressed = String.fromCharCode(keyCode);

	console.log(keyPressed, keyCode);

	if (keyPressed == "Q"){
		$("#btn-c0r0").focus().click();
	} else if (keyPressed == "A") {
		$("#btn-c0r1").focus().click();
	} else if (keyPressed == "Z") {
		$("#btn-c0r2").focus().click();
	} else if (keyPressed == "W"){
		$("#btn-c1r0").focus().click();
	} else if (keyPressed == "S") {
		$("#btn-c1r1").focus().click();
	} else if (keyPressed == "X") {
		$("#btn-c1r2").focus().click();
	} else if (keyPressed == "E"){
		$("#btn-c2r0").focus().click();
	} else if (keyPressed == "D") {
		$("#btn-c2r1").focus().click();
	} else if (keyPressed == "C") {
		$("#btn-c2r2").focus().click();
	} else if (keyPressed == "R"){
		$("#btn-c3r0").focus().click();
	} else if (keyPressed == "F") {
		$("#btn-c3r1").focus().click();
	} else if (keyPressed == "V") {
		$("#btn-c3r2").focus().click();
	} else if (keyPressed == "T") {
		$("#btn-c4r0").focus().click();
	} else if (keyPressed == " ") {
		$("#btn-c4r2").focus().click();
	} else if (keyPressed == "1"){
		$("#btn-c5r0").focus().click();
	} else if (keyPressed == "2") {
		$("#btn-c5r1").focus().click();
	} else if (keyPressed == "3") {
		$("#btn-c5r2").focus().click();
	} else {
		;
	}
}
	