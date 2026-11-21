/* the historical B compound assignments: =+ =- =* =/ =% =<< =>> and the relational ones */
main() {
  auto x, y, v[3];
  x = 10;
  x =+ 5;  printf("%d*n", x);   /* 15 */
  x =- 3;  printf("%d*n", x);   /* 12 */
  x =* 2;  printf("%d*n", x);   /* 24 */
  x =/ 4;  printf("%d*n", x);   /* 6 */
  x =% 4;  printf("%d*n", x);   /* 2 */
  x =<< 3; printf("%d*n", x);   /* 16 */
  x =>> 1; printf("%d*n", x);   /* 8 */
  x =| 3;  printf("%d*n", x);   /* 11 */
  x =& 6;  printf("%d*n", x);   /* 2 */
  y = 5; y =< 3;   printf("%d", y);
  y = 5; y =<= 5;  printf("%d", y);
  y = 5; y => 3;   printf("%d", y);
  y = 5; y =>= 6;  printf("%d", y);
  y = 5; y === 5;  printf("%d", y);
  y = 5; y =!= 5;  printf("%d*n", y);
  v[1] = 4; v[1] =* 3; v[1] =- 2;
  printf("%d*n", v[1]);        /* 10 */
}
