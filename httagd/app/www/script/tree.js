import { putTag } from "./api.js";
import { encodePutTag } from "./tagl.js";

/***** Functions navigating and manipulating the tagspace tree  *****/

/**
* Get the base URL (scheme + host + optional port), cache on first use
* @returns {string}
*/
function getBaseURL() {
	if (!getBaseURL.cached) {
		const loc = window.location;
		getBaseURL.cached = loc.origin || (loc.protocol + "//" + loc.host);
	}
	return getBaseURL.cached;
}

/**
* Initialize tree navigation buttons for adding children
* @param {HTMLElement} container — Optional container to limit scope
*/
export function setupAddChild(container = document) {
	container.querySelectorAll("#sidebar nav li").forEach(li => {
		const link = li.querySelector("a");
		if (!link) return;

		// Create the + button
		const addBtn = document.createElement("button");
		addBtn.textContent = "＋";
		addBtn.classList.add("add-child-btn");

		// Wrap <a> and button inside a span for hover styling
		const wrapper = document.createElement("span");
		wrapper.classList.add("add-child-wrapper");

		link.replaceWith(wrapper);
		wrapper.appendChild(link);
		wrapper.appendChild(addBtn);

		// Click event: show popup form
		addBtn.addEventListener("click", (e) => {
			e.preventDefault();
			openAddChildForm(addBtn, link);
		});
	});

	document.getElementById("add-child-form")
		?.addEventListener("submit", submitAddChild);
}

/**
* Show the add-child form near the clicked button
* @param {HTMLElement} button — The + button clicked
* @param {HTMLElement} link — The parent <a> tag for this node
*/
export function openAddChildForm(button, link) {
	const form = document.getElementById("add-child-form");
	const subId = document.getElementById("add-child-id");
	const superInput = document.getElementById("add-child-super");

	const rect = button.getBoundingClientRect();
	form.style.top = `${rect.bottom + window.scrollY}px`;
	form.style.left = `${rect.left + window.scrollX}px`;

	superInput.value = link.textContent.trim();
	subId.value = "";
	form.style.display = "block";
	subId.focus();
}

/**
* Handle form submission for creating new child entity
*/
export async function submitAddChild(event) {
	event.preventDefault();

	const subId = document.getElementById("add-child-id").value.trim();
	const superId = document.getElementById("add-child-super").value.trim();
	const form = document.getElementById("add-child-form");

	if (!subId || !superId) return false;

	try {
		// Generate the TAGL statement and submit it
		// TODO don't hardcode _sub, get the relation from the UI
		await putTag(subId, encodePutTag(subId, "_sub", superId));
		location.reload(); // TODO: Replace with dynamic partial update in future
	} catch (err) {
		alert(`Error: ${err.message}`);
	}

	form.style.display = "none";
	return false;
}

/**
* Hide form when pressing Escape or clicking outside
*/
export function setupAddChildCancel() {
	const form = document.getElementById("add-child-form");
	document.addEventListener("keydown", (e) => {
		if (e.key === "Escape") {
			form.style.display = "none";
		}
	});

	document.addEventListener("mousedown", (e) => {
		if (!form.contains(e.target)) {
			form.style.display = "none";
		}
	});
}

// Internal tree navigation state
const tree = {
	superLink: null,
	idLink: null,
	prevLink: null,
	nextLink: null,
	childLink: null
};

/**
* Populate tree object with link targets for keyboard navigation
*/
export function setupTreeNavigation() {
	tree.superLink = document.querySelector(".tree li.super a")?.href || null;
	const idLi = document.querySelector(".tree li.id");
	tree.idLink = idLi?.querySelector("a")?.href || null;
	tree.prevLink = idLi?.previousElementSibling?.querySelector("a")?.href || null;
	tree.nextLink = idLi?.parentElement?.querySelector("li.id ~ li.sibling a")?.href || null;
	tree.childLink = document.querySelector(".tree li.child a")?.href || null;
}

/**
* Setup Ctrl/Meta + arrow key keyboard navigation
*/
export function setupKeyboardNavigation() {
	document.addEventListener("keydown", (e) => {
		if (!e.ctrlKey && !e.metaKey) return;

		console.log("keydown", e.key);
		console.log("tree", tree);
		switch (e.key) {
			case "ArrowLeft":
				if (tree.superLink)
					location.href = tree.superLink;
				break;
			case "ArrowUp":
				if (tree.prevLink)
					location.href = tree.prevLink;
				break;
			case "ArrowRight":
				if (tree.childLink)
					location.href = tree.childLink;
				break;
			case "ArrowDown":
				if (tree.nextLink)
					location.href = tree.nextLink;
				break;
		}
	});
}

/**
* Populate keyboard shortcut hints (Ctrl+arrows)
*/
export function updateNavHint() {
	const modKey = navigator.platform.includes("Mac") ? "⌘" : "Ctrl";
	const hints = [];

	if (tree["superLink"]) hints.push(`${modKey} + ← parent`);
	if (tree["prevLink"]) hints.push(`${modKey} + ↑ previous`);
	if (tree["nextLink"]) hints.push(`${modKey} + ↓ next`);
	if (tree["childLink"]) hints.push(`${modKey} + → child`);

	const hintBox = document.getElementById("nav-hint");
	if (hintBox) hintBox.innerHTML = hints.join("<br>");
}

/*\
|*| Call all setup functions is this module.
\*/
export function setupTree() {
	setupAddChild();
	setupAddChildCancel();
	setupTreeNavigation();
	setupKeyboardNavigation();
	updateNavHint();
}
