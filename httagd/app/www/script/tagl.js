// scripts/tagl.js

/**
 * Creates a TAGL PUT tag statement
 *
 * @param {string} tagId - The tag being defined (left side)
 * @param {string} subRel - The subordinate relation
 * @param {string} superId - The superordinate tag
 * @returns {string} - A PUT tag TAGL statement
 */
export function encodePutTag(tagId, subRel, superId) {
	return `>> ${tagId} ${subRel} ${superId}`;
}

/**
 * Creates a TAGL PUT tag relation statement
 *
 * @param {string} subject - The tag being predicated upon
 * @param {string} predicate - The relation and the tag related to
 * @returns {string} - A PUT tag relation TAGL statement
 */
export function encodePutPredicate(subject, predicate) {
	return `>> ${subject} ${predicate}`;
}
