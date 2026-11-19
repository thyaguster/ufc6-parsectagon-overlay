// Parsectagon P emblem from src/assets/parsectagon-ufc-6.svg.
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
class MakeIcon {
 static GraphicsPath Rounded(float x,float y,float w,float h,float r){var p=new GraphicsPath();p.AddArc(x,y,r*2,r*2,180,90);p.AddArc(x+w-r*2,y,r*2,r*2,270,90);p.AddArc(x+w-r*2,y+h-r*2,r*2,r*2,0,90);p.AddArc(x,y+h-r*2,r*2,r*2,90,90);p.CloseFigure();return p;}
 static GraphicsPath FitText(string value,FontFamily family,FontStyle style,RectangleF box){
  var p=new GraphicsPath();p.AddString(value,family,(int)style,100,PointF.Empty,StringFormat.GenericTypographic);var bounds=p.GetBounds();
  using(var transform=new Matrix()){transform.Translate(-bounds.X,-bounds.Y,MatrixOrder.Append);transform.Scale(box.Width/bounds.Width,box.Height/bounds.Height,MatrixOrder.Append);transform.Translate(box.X,box.Y,MatrixOrder.Append);p.Transform(transform);}return p;
 }
 static string Svg(GraphicsPath p){
  var text=new System.Text.StringBuilder();var points=p.PathPoints;var types=p.PathTypes;
  Func<float,string> n=delegate(float x){return x.ToString("0.###",System.Globalization.CultureInfo.InvariantCulture);};
  for(int i=0;i<points.Length;i++){
   int type=types[i]&7;
   if(type==0)text.Append("M"+n(points[i].X)+" "+n(points[i].Y));
   else if(type==1)text.Append("L"+n(points[i].X)+" "+n(points[i].Y));
   else if(type==3){text.Append("C"+n(points[i].X)+" "+n(points[i].Y)+" "+n(points[i+1].X)+" "+n(points[i+1].Y)+" "+n(points[i+2].X)+" "+n(points[i+2].Y));i+=2;}
   if((types[i]&128)!=0)text.Append("Z");
  }return text.ToString();
 }
 static void Main(string[] args){
  string folder=args[1];Directory.CreateDirectory(folder);int[] sizes={16,24,32,48,64,128,256};var images=new byte[sizes.Length][];
  using(var master=new Bitmap(1024,1024,PixelFormat.Format32bppArgb)){
   using(var g=Graphics.FromImage(master)){
    g.SmoothingMode=SmoothingMode.AntiAlias;g.Clear(Color.Transparent);
    g.TranslateTransform(0,0);g.ScaleTransform(1024f/96f,1024f/96f);
    PointF[] outer={new PointF(29,5),new PointF(67,5),new PointF(91,29),new PointF(91,67),new PointF(67,91),new PointF(29,91),new PointF(5,67),new PointF(5,29)};
    PointF[] inner={new PointF(31,13),new PointF(65,13),new PointF(83,31),new PointF(83,65),new PointF(65,83),new PointF(31,83),new PointF(13,65),new PointF(13,31)};
    using(var dark=new SolidBrush(Color.FromArgb(19,21,26)))using(var red=new Pen(Color.FromArgb(230,74,80),5))using(var pale=new Pen(Color.FromArgb(56,255,255,255),2))using(var white=new SolidBrush(Color.FromArgb(244,244,246))){
     g.FillPolygon(dark,outer);g.DrawPolygon(red,outer);g.DrawPolygon(pale,inner);
     using(var p=new GraphicsPath(FillMode.Alternate)){
      p.StartFigure();p.AddLine(27,23,56,23);p.AddBezier(56,23,71,23,79,31,79,44);p.AddBezier(79,44,79,57,71,65,56,65);p.AddLine(56,65,43,65);p.AddLine(43,65,43,78);p.AddLine(43,78,27,78);p.CloseFigure();
      p.StartFigure();p.AddLine(43,37,55,37);p.AddBezier(55,37,60,37,63,39,63,44);p.AddBezier(63,44,63,49,60,52,55,52);p.AddLine(55,52,43,52);p.CloseFigure();
      g.FillPath(white,p);
     }
     g.DrawLine(red,16,72,29,85);
    }
    File.WriteAllText(Path.Combine(folder,"overlay.svg"),"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 96 96\"><path d=\"M29 5H67L91 29V67L67 91H29L5 67V29Z\" fill=\"#13151a\" stroke=\"#e64a50\" stroke-width=\"5\"/><path d=\"M31 13H65L83 31V65L65 83H31L13 65V31Z\" fill=\"none\" stroke=\"#ffffff\" stroke-opacity=\".22\" stroke-width=\"2\"/><path fill=\"#f4f4f6\" fill-rule=\"evenodd\" d=\"M27 23h29c15 0 23 8 23 21S71 65 56 65H43v13H27V23Zm16 14v15h12c5 0 8-3 8-8s-3-7-8-7H43Z\"/><path d=\"M16 72 29 85\" stroke=\"#e64a50\" stroke-width=\"4\"/></svg>"+Environment.NewLine);
   }
   for(int i=0;i<sizes.Length;i++)using(var frame=new Bitmap(sizes[i],sizes[i],PixelFormat.Format32bppArgb)){
    using(var g=Graphics.FromImage(frame)){g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.DrawImage(master,new Rectangle(0,0,sizes[i],sizes[i]));}
    using(var stream=new MemoryStream()){frame.Save(stream,ImageFormat.Png);images[i]=stream.ToArray();}
    if(sizes[i]==256)frame.Save(Path.Combine(folder,"overlay.png"),ImageFormat.Png);
   }
  }
  using(var file=File.Create(Path.Combine(folder,"overlay.ico")))using(var w=new BinaryWriter(file)){
   w.Write((ushort)0);w.Write((ushort)1);w.Write((ushort)sizes.Length);int offset=6+16*sizes.Length;
   for(int i=0;i<sizes.Length;i++){w.Write((byte)(sizes[i]==256?0:sizes[i]));w.Write((byte)(sizes[i]==256?0:sizes[i]));w.Write((byte)0);w.Write((byte)0);w.Write((ushort)1);w.Write((ushort)32);w.Write(images[i].Length);w.Write(offset);offset+=images[i].Length;}
   foreach(var data in images)w.Write(data);
  }
 }
}
