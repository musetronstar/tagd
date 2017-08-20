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
	"super_object_link": null,
	"id_link": null,
	"prev_link": null,
	"next_link": null,
	"child": []
};

$(".tree li").each(function(i) {
	var cls = $(this).attr('class');
	//console.log('li('+i+','+typeof($( this ))+'): '+cls);
});

$(".tree li.sub > a").each(function(i) {
	tree["super_object_link"] = $(this).attr('href');
	//console.log('sub('+i+','+typeof($( this ))+'): '+tree['super_object_link']);
});

$(".tree li.prev > a").each(function(i) {
	tree["prev_link"] = $(this).attr('href');
	//console.log('prev('+i+','+typeof($( this ))+'): '+tree['prev_link']);
});

$(".tree li.id > a").each(function(i) {
	tree["id_link"] = $(this).attr('href');
	//console.log('id('+i+','+typeof($( this ))+'): '+tree['id_link']);
});

$(".tree li.next > a").each(function(i) {
	tree["next_link"] = $(this).attr('href');
	//console.log('next('+i+','+typeof($( this ))+'): '+tree['next_link']);
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
			window.location.href = tree['super_object_link'];
			//console.log("left: " + tree['super_object_link']);
			break;
		case 38:
			if (tree['prev_link'])
				window.location.href = tree['prev_link'];
			//console.log("up: " + tree['prev_link']);
			break;
		case 39:
			if (tree['child'][0])
				window.location.href = tree['child'][0];
			//console.log("right: " + tree['child']);
			break;
		case 40:
			if (tree['next_link'])
				window.location.href = tree['next_link'];
			//console.log("down: " + tree['next_link']);
			break;
	}
	return true;
});
</script>
