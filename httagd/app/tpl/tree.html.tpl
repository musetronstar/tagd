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
<li class="sub"><a href="{{super_object_lnk}}">{{super_object}}</a>
 <ul>
 {{#has_prev}}<li class="prev"><a href="{{prev_lnk}}">{{prev}}</a></li>{{/has_prev}}
  <li class="id"><a href="{{id_lnk}}">{{id}}</a>
  {{#has_children}}
   <ul>
  	{{#children}}
      <li class="child"><a href="{{child_lnk}}">{{child}}</a></li>
  	{{/children}}
    </ul>
  {{/has_children}}
   </li>
  {{#has_next}}<li class="next"><a href="{{next_lnk}}">{{next}}</a></li>{{/has_next}}
 </ul>
</li>
</ul>
<script type="text/javascript">

var tree = {
	"superObj": null,
	"idLink": null,
	"prevLink": null,
	"nextLink": null,
	"child": []
};

tree["superObj"] = document.querySelector(".tree li.sub > a").getAttribute('href');
tree["prevLink"] = document.querySelector(".tree li.prev > a").getAttribute('href');
tree["idLink"] = document.querySelector(".tree li.id > a").getAttribute('href');
tree["nextLink"] = document.querySelector(".tree li.next > a").getAttribute('href');

document.querySelectorAll(".tree li.child > a").forEach(function(e) {
  tree["child"].push(e.getAttribute('href'));
});

document.onkeydown = function(e){
	switch(e.keyCode) {
		case 37:
			window.location.href = tree['superObj'];
			break;
		case 38:
			if (tree['prevLink'])
				window.location.href = tree['prevLink'];
			break;
		case 39:
			if (tree['child'][0])
				window.location.href = tree['child'][0];
			break;
		case 40:
			if (tree['nextLink'])
				window.location.href = tree['nextLink'];
			break;
	}
	return true;
};
</script>
