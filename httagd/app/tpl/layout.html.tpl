<!DOCTYPE HTML>
<html>
<head>
<meta charset='utf-8'>
<title>{{title}}</title>
<link rel="stylesheet" href="/_file/style/main.css">
<link rel="stylesheet" href="http://cdnjs.cloudflare.com/ajax/libs/jquery.colorbox/1.4.33/example1/colorbox.min.css" type="text/css">
<script src="http://cdnjs.cloudflare.com/ajax/libs/jquery/2.1.0/jquery.min.js"></script>
<script src="http://cdnjs.cloudflare.com/ajax/libs/jquery.colorbox/1.4.33/jquery.colorbox.js"></script>
</head>
<body>
<header>
 <span id="logo">
  <bigger><strong>tagd</strong></bigger>
 </span>
 <span id="search">
  <form id="search-form" method="GET" action="/">
{{#query_options}}
 {{#query_option}}
 <input type="hidden" name="{{query_key}}" value="{{query_value}}">
 {{/query_option}}
{{/query_options}}
   <input id="terms" name="q" type="text" required>
   <button type="submit">Search</button>
  </form>
 </span>
 <hr>
</header>

{{>main_html_tpl}}

</body>
</html>
