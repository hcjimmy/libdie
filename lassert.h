/* Leveled assert.
 * Copyright (C) 2023  hcjimmy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <assert.h>

#define ASSERT_LVL_OFF			0
#define ASSERT_LVL_FAST 		1	// Mostly simple checks that are O(1)
#define ASSERT_LVL_PRETTY_FAST 		2	// Simlar to fast, but may be slower or called alot.
						// (when in doubt, use pretty fast).
#define ASSERT_LVL_MEDIUM 		3	// Mostly O(n).
#define ASSERT_LVL_PRETTY_SLOW 		4
#define ASSERT_LVL_SLOW 		5	// Mostly O(n^2) or worse.

// Define the default level maximum level.
#ifndef assert_level
	#define assert_level ASSERT_LVL_PRETTY_FAST
#endif

// Run assert if the level defined at compilation is high enough.
// In the case it isn't, we trust any modern compiler will ignore the if statement
// entirely, since it should always amount to false (when level is known).
#define lassert(assertion, level) do {		\
	if(assert_level >= (level))		\
		assert((assertion));		\
} while(0)


