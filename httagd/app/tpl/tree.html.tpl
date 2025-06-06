<style>
ul {
    margin: 0.75em 0;
    padding: 0 1em;
    list-style: none;
}

.id > a { font-weight: bold; }
li.id:before { 
    content: "";
    border-color: transparent #111;
    border-style: solid;
    border-width: 0.35em 0 0.35em 0.45em;
    display: block;
    height: 0;
    width: 0;
    left: -1em;
    top: 0.9em;
    position: relative;
}

.add-child-wrapper .add-child-btn {
  display: none;
  margin-left: 0.5em;
  font-size: 0.8em;
}

.add-child-wrapper:hover .add-child-btn {
  display: inline;
}
</style>
<ul class="tree">
<li class="super"><a href="{{super_object_lnk}}">{{super_object}}</a>
 <ul>
 {{#siblings}}
  {{#this_tag}}
   <li class="sibling id"><a href="{{this_tag_id_lnk}}">{{this_tag_id}}</a>
   {{#has_children}}
    <ul>
    {{#children}}
     <li class="child"><a href="{{child_lnk}}">{{child}}</a></li>
    {{/children}}
    </ul>
   {{/has_children}}
   </li>
  {{/this_tag}}
  {{#sibling}}
   <li class="sibling"><a href="{{sibling_id_lnk}}">{{sibling_id}}</a></li>
  {{/sibling}}
 {{/siblings}}
 </ul>
</li>
</ul>
<script type="text/javascript">

var tree = {
	"superLink": null,
	"idLink": null,
	"prevLink": null,
	"nextLink": null,
	"child": []
};

tree["superLink"] = document.querySelector(".tree li.super > a").getAttribute('href') || null;
var idLi = document.querySelector(".tree li.id");
tree["idLink"] = idLi.querySelector("a").getAttribute('href');
tree["prevLink"] = idLi.previousElementSibling?.querySelector("a")?.getAttribute('href') || null;
tree["nextLink"] = idLi.parentElement.querySelector("li.id ~ li.sibling a")?.getAttribute("href") || null;
tree["childLink"] = document.querySelector(".tree li.child a")?.getAttribute('href') || null;

document.onkeydown = function(e){
	// Require either Control (Windows/Linux) or Meta (Command on macOS)
	if (!e.ctrlKey && !e.metaKey) return true;

	switch(e.keyCode) {
		case 37: // Ctrl/Cmd + Left Arrow
			if (tree["superLink"])
				window.location.href = tree["superLink"];
			break;
		case 38: // Ctrl/Cmd + Up Arrow
			if (tree["prevLink"])
				window.location.href = tree["prevLink"];
			break;
		case 39: // Ctrl/Cmd + Right Arrow
			if (tree["childLink"])
				window.location.href = tree["childLink"];
			break;
		case 40: // Ctrl/Cmd + Down Arrow
			if (tree["nextLink"])
				window.location.href = tree["nextLink"];
			break;
	}
	return true;
};
</script>
<form id="add-child-form" style="display:none; position:absolute; z-index:10000; background:#fff; border:1px solid #ccc; padding:0.5em; box-shadow:0 0 5px rgba(0,0,0,0.1);" onsubmit="return submitAddChild(event)">
  <input type="text" id="add-child-name" placeholder="New entity name" required autofocus>
  <input type="hidden" id="add-child-parent">
  <button type="submit">Add</button>
</form>
<script>
document.querySelectorAll("#sidebar nav li").forEach(li => {
  const link = li.querySelector("a");
  if (!link) return;

  // Create + button
  const addBtn = document.createElement("button");
  addBtn.textContent = "＋";
  addBtn.classList.add("add-child-btn");

  // Wrap the link and button in a span container
  const wrapper = document.createElement("span");
  wrapper.classList.add("add-child-wrapper");
  link.replaceWith(wrapper);
  wrapper.appendChild(link);
  wrapper.appendChild(addBtn);

  // Open popup form on click
  addBtn.addEventListener("click", (e) => {
    e.preventDefault();
    const form = document.getElementById("add-child-form");
    const nameInput = document.getElementById("add-child-name");
    const parentInput = document.getElementById("add-child-parent");

    const rect = addBtn.getBoundingClientRect();
    form.style.top = `${rect.bottom + window.scrollY}px`;
    form.style.left = `${rect.left + window.scrollX}px`;

    parentInput.value = link.textContent.trim();
    nameInput.value = "";
    form.style.display = "block";
    nameInput.focus();
  });
});

// Handle submission of the popup form
function submitAddChild(event) {
	event.preventDefault(); // Stop normal form submission

	const name = document.getElementById("add-child-name").value.trim();   // New entity
	const parent = document.getElementById("add-child-parent").value.trim(); // Superordinate entity
	const form = document.getElementById("add-child-form");

	if (!name || !parent) return false; // Guard clause

	// Format tagl assertion body: >> new_entity _is_a parent_entity
	const body = `>> ${name} _is_a ${parent}`;

	// Create and configure PUT request
	const xhr = new XMLHttpRequest();
	xhr.open("PUT", `http://localhost:2112/${encodeURIComponent(name)}`);
	xhr.setRequestHeader("Content-Type", "text/plain");

	// On successful request
	xhr.onload = () => {
		if (xhr.status >= 200 && xhr.status < 300) {
			location.reload();
		} else {
			alert(`Error: ${xhr.status}\n${xhr.responseText}`);
		}
	};

	// Handle network error
	xhr.onerror = () => alert("Network error");

	// Send the PUT request body
	xhr.send(body);

	// Hide the form after submitting
	form.style.display = "none";
	return false;
}

// Cancel the add-child form with Escape key
document.addEventListener("keydown", (e) => {
  const form = document.getElementById("add-child-form");
  if (e.key === "Escape" && form.style.display === "block") {
    form.style.display = "none";
  }
});

// Cancel the add-child form with click-outside
document.addEventListener("click", (e) => {
  const form = document.getElementById("add-child-form");
  const isFormOpen = form.style.display === "block";
  const isClickInsideForm = form.contains(e.target);
  const isAddButton = e.target.classList.contains("add-child-btn");

  if (isFormOpen && !isClickInsideForm && !isAddButton) {
    form.style.display = "none";
  }
});
</script>
<div id="nav-hint" style="
  position: fixed;
  bottom: 1em;
  left: 1em;
  background: rgba(255, 255, 255, 0.55);  /* translucent white background */
  border: 1px solid #ccc;
  padding: 0.5em 1em;
  font-family: sans-serif;
  font-size: 0.9em;
  color: #333;
  box-shadow: 0 0 5px rgba(0, 0, 0, 0.1);
  border-radius: 5px;
  z-index: 9999;
  pointer-events: none;
"></div>
<script type="text/javascript">
function updateNavHint() {
	const modKey = navigator.platform.includes('Mac') ? '⌘' : 'Ctrl';
	const hints = [];

	if (tree["superLink"])
		hints.push(`${modKey} + ← parent`);
	if (tree["prevLink"])
		hints.push(`${modKey} + ↑ previous`);
	if (tree["nextLink"])
		hints.push(`${modKey} + ↓ next`);
	if (tree["childLink"])
		hints.push(`${modKey} + → child`);

	document.getElementById("nav-hint").innerHTML = hints.join("<br>");
}

updateNavHint();
</script>
