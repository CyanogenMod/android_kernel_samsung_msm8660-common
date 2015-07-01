/*
 * Author: Jean-Pierre Rasquin <yank555.lu@gmail.com>
 *
 * Inspired by the work of Chad Froebel <chadfroebel@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

extern int s2w_switch;
extern int s2s_switch;
extern int s2w_lenient;

/* The global /sys/android_touch/ kobject for people to chain off of */
extern struct kobject *android_touch_kobj;

