const theme = localStorage.getItem("theme") || "dark";
if (theme == "light") {
	document.body.classList.add("latte");
}

document.getElementById("change-theme").onclick = function () {
	const theme = localStorage.getItem("theme") || "dark";
	if (theme == "light") {
		localStorage.setItem("theme", "dark");
		document.body.classList.remove("latte");
	} else {
		localStorage.setItem("theme", "light");
		document.body.classList.add("latte");
	}
}
