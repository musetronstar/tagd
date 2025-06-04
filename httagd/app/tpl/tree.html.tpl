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
</style>
<ul class="tree">
<li class="super"><a href="{{super_object_lnk}}">{{super_object}}</a>
 <ul>
 {{#siblings}}
  {{#this_tag}}
   <li class="sibling id"><a href="{{this_tag_id_lnk}}">{{this_tag_id}}</a></li>
   {{#has_children}}
    <ul>
    {{#children}}
     <li class="child"><a href="{{child_lnk}}">{{child}}</a></li>
    {{/children}}
    </ul>
   {{/has_children}}
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
