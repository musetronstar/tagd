import { postTAGL } from "./api.js";

/**
 * Setup tagsh "tagd shell"
 */
function setupTagshForm() {
	const tagshForm = document.getElementById("tagsh-form");
	tagshForm?.addEventListener("submit", submitTagsh);
}

/**
 * Submit the tagsh-form via POST
 */
export async function submitTagsh(event) {
	event.preventDefault();

	const tagl = document.getElementById("tagsh-input")?.value.trim() || null;
	console.log("submitTagsh: " + tagl);
	if (!tagl) return false;

	try {
		// Generate the TAGL statement and submit it
		// TODO don't hardcode _sub, get the relation from the UI
		const res = await postTAGL(tagl);
	} catch (err) {
		alert(`Error: ${err.message}`);
	}

	return false;
}

/*\
|*| Call all setup functions is this module.
\*/
export function setupTagsh() {
	if (setupTagsh.isSetup) return; // already set up

	setupTagshForm();

	setupTagsh.isSetup = true;
}
