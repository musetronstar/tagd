import { putTag } from "./api.js";
import { encodePutPredicate } from "./tagl.js";

/**
 * Setup add-predicate button and form
 */
export function setupAddPredicate() {
	const tagContainer = document.getElementById("tag-container");
	if (!tagContainer) return;

	// Create the button
	const addPredBtn = document.createElement("button");
	addPredBtn.textContent = "＋";
	addPredBtn.classList.add("add-predicate-btn");
	tagContainer.appendChild(addPredBtn);

	// Handle click: show form near button
	addPredBtn.addEventListener("click", (e) => {
		e.preventDefault();
		openAddPredicateForm(addPredBtn);
	});

	document.getElementById("add-predicate-form")
		?.addEventListener("submit", submitAddPredicate);
}

/**
 * Show the add-predicate form near the button
 */
export function openAddPredicateForm(button) {
	const form = document.getElementById("add-predicate-form");
	const input = document.getElementById("add-predicate-input");
	const tagId = document.getElementById("tag-id");

	if (!form || !input || !tagId) return;

	// Position the form
	const rect = button.getBoundingClientRect();
	form.style.top = `${rect.bottom + window.scrollY}px`;
	form.style.left = `${rect.left + window.scrollX}px`;

	form.style.display = "block";
	input.value = "";
	input.focus();
}

/**
 * Submit the add-predicate form via POST
 */
export async function submitAddPredicate(event) {
	event.preventDefault();

	const tagId = document.getElementById("tag-id")?.textContent.trim() || null;
	const predicate = document.getElementById("add-predicate-input")?.value.trim() || null;
	if (!tagId || !predicate) return false;

	try {
		// Generate the TAGL statement and submit it
		// TODO don't hardcode _sub, get the relation from the UI
		await putTag(tagId, encodePutPredicate(tagId, predicate));
		location.reload(); // TODO: Replace with dynamic partial update in future
	} catch (err) {
		alert(`Error: ${err.message}`);
	}

	document.getElementById("add-predicate-form").style.display = "none";
	return false;
}

/*\
|*| Call all setup functions is this module.
\*/
export function setupTag() {
	setupAddPredicate();
}
