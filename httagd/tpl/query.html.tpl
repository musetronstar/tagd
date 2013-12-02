<!DOCTYPE HTML>
<html>
<head>
<meta charset='utf-8'>
<title>{{query}}</title>
</head>
<body>

 <h1>{{interrogator}}</h1>
 &rarr;
 <a href="{{super_relator_lnk}}">{{super_relator}}</a>
 <a href="{{super_object_lnk}}">{{super_object}}</a>
 <ul>
{{#relations}}{{#relation}}
   <li>
    <a href="{{relator_lnk}}">{{relator}}</a>
    <a href="{{object_lnk}}">{{object}}</a>
	 {{#has_modifier}}= {{modifier}}{{/has_modifier}}
  </li>
{{/relation}}{{/relations}}
 </ul>

 <p><em>{{num_results}}</em> results</p>
 <hr>

{{#results}}
<ol>
 {{#result}}
 <li>
 <h2><a href="{{res_id_lnk}}">{{res_id}}</a></h2>
 &rarr;
 <a href="{{res_super_relator_lnk}}">{{res_super_relator}}</a>
 <a href="{{res_super_object_lnk}}">{{res_super_object}}</a>
{{#res_relations}}
 <ul>
 {{#res_relation}}
  <li>
  <a href="{{res_relator_lnk}}">{{res_relator}}</a>
  <a href="{{res_object_lnk}}">{{res_object}}</a>
  {{#res_has_modifier}} = {{res_modifier}}{{/res_has_modifier}}</li>
 {{/res_relation}}
 </ul>
{{/res_relations}}
 </li>
 {{/result}}
 </ol>
{{/results}}

 <footer><pre>{{errors}}</pre></footer>
</body>
</html>
