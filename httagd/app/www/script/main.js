/***** Unified behavior module for TAGD UI *****/

import { setupTree } from './tree.js';
import { setupTag } from './tag.js';
import { setupTagsh } from './tagsh.js';

document.addEventListener("DOMContentLoaded", () => {
	setupTree();
	setupTag();
	setupTagsh();
});
