
function refreshTagHTML() {
	var req = new XMLHttpRequest();

	req.onload = function () {
		if (req.status == 200) {
		} else {
      document.getElementById("tag_container").innerHTML = req.response;
			document.getElementById("tagl_error").innerHTML = req.response;
		}
	}

	req.open("GET", "{{REQUEST_URL_VIEW_TAG_HTML}}");
	req.send();
}

function postTAGL() {
	var req = new XMLHttpRequest();

	req.onload = function () {
		if (req.status == 200) {
			refreshTagHTML();
		} else {
			document.getElementById("tagl_error").innerHTML = req.response;
		}
	}

	taglURL = document.getElementById('tagl_url').value;
	req.open("POST", taglURL);
	req.setRequestHeader("Content-Type", "text/plain;charset=UTF-8");
	req.send(tagl_form.tagl.value);
}


