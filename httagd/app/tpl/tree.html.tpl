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

$(".tree li").each(function(i) {
	var cls = $(this).attr('class');
	//console.log('li('+i+','+typeof($( this ))+'): '+cls);
});

$(".tree li.sub > a").each(function(i) {
	tree["superObj"] = $(this).attr('href');
	//console.log('sub('+i+','+typeof($( this ))+'): '+tree['superObj']);
});

$(".tree li.prev > a").each(function(i) {
	tree["prevLink"] = $(this).attr('href');
	//console.log('prev('+i+','+typeof($( this ))+'): '+tree['prevLink']);
});

$(".tree li.id > a").each(function(i) {
	tree["idLink"] = $(this).attr('href');
	//console.log('id('+i+','+typeof($( this ))+'): '+tree['idLink']);
});

$(".tree li.next > a").each(function(i) {
	tree["nextLink"] = $(this).attr('href');
	//console.log('next('+i+','+typeof($( this ))+'): '+tree['nextLink']);
});

$(".tree li.child > a").each(function(i) {
	tree["child"][i] = $(this).attr('href');
	//console.log('child('+i+','+typeof($( this ))+'): '+tree['child']);
});

/*
$(document).ready(function() {
	console.log(JSON.stringify(tree));
});
*/

$(document).keydown(function(e){
	switch(e.keyCode) {
		case 37:
			window.location.href = tree['superObj'];
			//console.log("left: " + tree['superObj']);
			break;
		case 38:
			if (tree['prevLink'])
				window.location.href = tree['prevLink'];
			//console.log("up: " + tree['prevLink']);
			break;
		case 39:
			if (tree['child'][0])
				window.location.href = tree['child'][0];
			//console.log("right: " + tree['child']);
			break;
		case 40:
			if (tree['nextLink'])
				window.location.href = tree['nextLink'];
			//console.log("down: " + tree['nextLink']);
			break;
	}
	return true;
});
</script>
