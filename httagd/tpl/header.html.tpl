<!DOCTYPE HTML>
<html>
<head>
<meta charset='utf-8'>
<title>{{title}}</title>
<style>

/************************************************************************************
STRUCTURE
*************************************************************************************/
#pagewrap {
	width: 100%;
	margin: 0 auto;
}

/************************************************************************************
HEADER
*************************************************************************************/
header {
	position: relative;
}
header #logo ,
header #search {
	float: left;
	margin-bottom: 0.3671em;
}
header #logo {
	font-size: 1.3671em;
	margin-right: 13.333672%;
}
header #search {
	width: 47.333672%;
}
#search input {
	width: 83%;
}
#search button {
	width: 4.7em;
}
header hr {
	margin: 3.671em 0 0 0;
	clear: both;
}

/* site logo */
#site-logo {
	position: absolute;
	top: 10px;
}
#site-logo a {
	font: bold 30px/100% Arial, Helvetica, sans-serif;
	color: #fff;
	text-decoration: none;
}

/* site description */
#site-description {
	font: italic 100%/130% "Times New Roman", Times, serif;
	color: #fff;
	position: absolute;
	top: 55px;
}

/************************************************************************************
MAIN NAVIGATION
*************************************************************************************/
#main-nav {
	width: 100%;
	background: #ccc;
	margin: 0;
	padding: 0;
	position: absolute;
	left: 0;
	bottom: 0;
	z-index: 100;
	/* gradient */
	background: #6a6a6a url(images/nav-bar-bg.png) repeat-x;
	background: -webkit-gradient(linear, left top, left bottom, from(#b9b9b9), to(#6a6a6a));
	background: -moz-linear-gradient(top,  #b9b9b9,  #6a6a6a);
	background: linear-gradient(-90deg, #b9b9b9, #6a6a6a);
	/* rounded corner */
	-webkit-border-radius: 8px;
	-moz-border-radius: 8px;
	border-radius: 8px;
	/* box shadow */
	-webkit-box-shadow: inset 0 1px 0 rgba(255,255,255,.3), 0 1px 1px rgba(0,0,0,.4);
	-moz-box-shadow: inset 0 1px 0 rgba(255,255,255,.3), 0 1px 1px rgba(0,0,0,.4);
	box-shadow: inset 0 1px 0 rgba(255,255,255,.3), 0 1px 1px rgba(0,0,0,.4);
}


/************************************************************************************
SIDEBAR
*************************************************************************************/
#sidebar {
	width: 31%;
	float: left;
	/* margin: 30px 0 30px; */
}

/************************************************************************************
CONTENT
*************************************************************************************/
#content {
	background: #fff;
	/* margin: 30px 0 30px; 
	padding: 20px 35px; */
	width: 63%;
	float: right;
	/* rounded corner 
	-webkit-border-radius: 8px;
	-moz-border-radius: 8px;
	border-radius: 8px;*/
	/* box shadow 
	-webkit-box-shadow: 0 1px 3px rgba(0,0,0,.4);
	-moz-box-shadow: 0 1px 3px rgba(0,0,0,.4);
	box-shadow: 0 1px 3px rgba(0,0,0,.4);*/
}


/************************************************************************************
FOOTER
*************************************************************************************/
footer {
	clear: both;
	color: #ccc;
	font-size: 85%;
}
footer a {
	color: #fff;
}

/************************************************************************************
CLEARFIX
*************************************************************************************/
.clearfix:after { visibility: hidden; display: block; font-size: 0; content: " "; clear: both; height: 0; }
.clearfix { display: inline-block; }
.clearfix { display: block; zoom: 1; }
</style>

<!-- media queries css -->
<style>
/************************************************************************************
smaller than 980
*************************************************************************************/
@media screen and (max-width: 980px) {

	/* pagewrap */

	/* content */
	#content {
		padding: 3% 4%;
	}

	/* sidebar */

	/* embedded videos */
	.video embed,
	.video object,
	.video iframe {
		width: 100%;
		height: auto;
		min-height: 300px;
	}

}

/************************************************************************************
smaller than 650
*************************************************************************************/
@media screen and (max-width: 650px) {

	/* header */
	header {
		height: auto;
	}

	/* main nav */
	#main-nav {
		position: static;
	}

	/* site logo */
	#site-logo {
		margin: 15px 100px 5px 0;
		position: static;
	}

	/* site description */
	#site-description {
		margin: 0 0 15px;
		position: static;
	}

	/* content */
	#content {
		width: auto;
		float: none;
		margin: 20px 0;
	}

	/* sidebar */
	#sidebar {
		width: 100%;
		margin: 0;
		float: none;
	}

	/* embedded videos */
	.video embed,
	.video object,
	.video iframe {
		min-height: 250px;
	}

}

/************************************************************************************
smaller than 560
*************************************************************************************/
@media screen and (max-width: 480px) {

	/* disable webkit text size adjust (for iPhone) */
	html {
		-webkit-text-size-adjust: none;
	}

	/* main nav */
	#main-nav a {
		font-size: 90%;
		padding: 10px 8px;
	}

</style>

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
  <form id="search-form" method="GET" action="{{request_url}}">
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
