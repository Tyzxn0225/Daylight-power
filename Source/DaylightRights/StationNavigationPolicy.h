#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

// Engine-independent navigation math, also exercised by the offline checks.
namespace StationNavigation
{
struct Point { double x=0,y=0; };
inline Point operator+(Point a,Point b){return {a.x+b.x,a.y+b.y};}
inline Point operator-(Point a,Point b){return {a.x-b.x,a.y-b.y};}
inline Point operator*(Point a,double b){return {a.x*b,a.y*b};}
inline double Length(Point a){return std::hypot(a.x,a.y);}
inline Point Unit(Point a){double d=Length(a);return d>1e-8?a*(1/d):Point{0,-1};}
struct Rect {double x=0,y=0,w=1,h=1;};
inline Point Project(Point world,Point center,Rect box,double cmPerPixel)
{return {box.x+box.w*.5+(world.x-center.x)/cmPerPixel,box.y+box.h*.5+(world.y-center.y)/cmPerPixel};}
inline bool Inside(Point p,Rect b,double inset=0)
{return p.x>=b.x+inset&&p.x<=b.x+b.w-inset&&p.y>=b.y+inset&&p.y<=b.y+b.h-inset;}
// Ray/rectangle intersection: off-map objectives keep their true bearing.
inline Point Edge(Point p,Rect b,double inset)
{
 const Point c{b.x+b.w*.5,b.y+b.h*.5},d=p-c;
 const double sx=(b.w*.5-inset)/std::max(std::abs(d.x),1e-8),sy=(b.h*.5-inset)/std::max(std::abs(d.y),1e-8);
 return c+d*std::min({1.,sx,sy});
}
inline bool Clip(Point& a,Point& b,Rect r)
{
 const Point d=b-a;double t0=0,t1=1;
 auto limit=[&](double p,double q){if(std::abs(p)<1e-9)return q>=0;double t=q/p;if(p<0){if(t>t1)return false;t0=std::max(t0,t);}else{if(t<t0)return false;t1=std::min(t1,t);}return true;};
 if(!limit(-d.x,a.x-r.x)||!limit(d.x,r.x+r.w-a.x)||!limit(-d.y,a.y-r.y)||!limit(d.y,r.y+r.h-a.y))return false;
 b=a+d*t1;a=a+d*t0;return true;
}
inline double SignedBearing(Point heading,Point toward)
{heading=Unit(heading);toward=Unit(toward);return std::atan2(heading.x*toward.y-heading.y*toward.x,heading.x*toward.x+heading.y*toward.y)*180/3.141592653589793;}
enum class Turn {Ahead,Left,Right,Behind};
inline Turn Direction(double angle){return std::abs(angle)>145?Turn::Behind:angle<-30?Turn::Left:angle>30?Turn::Right:Turn::Ahead;}
inline bool ShowOptional(bool seen,double distance,bool visible){return seen||(distance<=650&&visible);}
inline bool Replan(bool sameTarget,double since,double playerMoved,double targetMoved)
{return !sameTarget||since>=2.||since>=.55&&(playerMoved>=130||targetMoved>=100);}
inline bool Arrived(bool focused,double distance,bool visible,bool sameFloor)
{return focused||distance<=125&&visible&&sameFloor;}
struct EdgeLink {int a=0,b=0;};
// The caller supplies collision/floor validation; failed edges are never drawn.
template<class IsClear>
std::vector<int> Path(const std::vector<Point>& nodes,const std::vector<EdgeLink>& edges,IsClear clear)
{
 if(nodes.size()<2)return {};
 std::vector<std::vector<int>> links(nodes.size());
 for(auto e:edges)if(e.a>=0&&e.b>=0&&e.a<int(nodes.size())&&e.b<int(nodes.size())&&e.a!=e.b){links[e.a].push_back(e.b);links[e.b].push_back(e.a);}
 const double inf=std::numeric_limits<double>::infinity();std::vector<double> g(nodes.size(),inf);std::vector<int> parent(nodes.size(),-1);std::vector<bool> closed(nodes.size(),false);
 using Entry=std::pair<double,int>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> q;g[0]=0;q.push({Length(nodes[1]-nodes[0]),0});
 while(!q.empty())
 {
  int i=q.top().second;q.pop();if(closed[i])continue;closed[i]=true;
  if(i==1){std::vector<int> path;for(int k=1;k>=0;k=parent[k])path.push_back(k);std::reverse(path.begin(),path.end());return path;}
  for(int j:links[i])if(!closed[j]){double v=g[i]+Length(nodes[j]-nodes[i]);if(v<g[j]&&clear(i,j)){g[j]=v;parent[j]=i;q.push({v+Length(nodes[1]-nodes[j]),j});}}
 }
 return {};
}
}
