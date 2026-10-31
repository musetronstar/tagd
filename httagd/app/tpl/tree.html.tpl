<ul class="tree">
<li class="super"><a href="{{super_object_lnk:h}}">{{super_object:h}}</a>
 <ul>
 {{#siblings}}
  {{#this_tag}}
   <li class="sibling id"><a href="{{this_tag_id_lnk:h}}">{{this_tag_id:h}}</a>
   {{#has_children}}
    <ul>
    {{#children}}
     <li class="child"><a href="{{child_lnk:h}}">{{child:h}}</a></li>
    {{/children}}
    </ul>
   {{/has_children}}
   </li>
  {{/this_tag}}
  {{#sibling}}
   <li class="sibling"><a href="{{sibling_id_lnk:h}}">{{sibling_id:h}}</a></li>
  {{/sibling}}
 {{/siblings}}
 </ul>
</li>
</ul>