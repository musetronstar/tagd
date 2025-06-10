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
export function submitAddPredicate(event) {
	event.preventDefault();

	const input = document.getElementById("add-predicate-input");
	const tagId = document.getElementById("tag-id")?.textContent.trim();
	if (!input || !tagId) return false;

	const body = input.value.trim();
	if (!body) return false;

	const xhr = new XMLHttpRequest();
	xhr.open("POST", `${location.origin}/${encodeURIComponent(tagId)}`);
	xhr.setRequestHeader("Content-Type", "text/plain");

	xhr.onload = () => {
		if (xhr.status >= 200 && xhr.status < 300) {
			location.reload(); // TODO: dynamic refresh in future
		} else {
			alert(`Error: ${xhr.status}\n${xhr.responseText}`);
		}
	};

	xhr.onerror = () => alert("Network error");
	xhr.send(body);

	document.getElementById("add-predicate-form").style.display = "none";
	return false;
}

/*\
|*| Call all setup functions is this module.
\*/
export function setupTag() {
	setupAddPredicate();
}
