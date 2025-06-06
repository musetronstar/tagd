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