using System.Windows;
using System.Windows.Controls;

namespace RawLab.Windows;

// Folder headers stay in the tree; photos share one or two equal-width columns.
public sealed class PhotoGridPanel : Panel
{
    private const double RowHeight=140;
    private static int Columns(double width)=>width>=248 ? 2 : 1;
    protected override Size MeasureOverride(Size available)
    {
        var width=double.IsFinite(available.Width) ? available.Width : 220;
        var columns=Columns(width);
        foreach(UIElement child in InternalChildren)child.Measure(new Size(width/columns,RowHeight));
        return new Size(width,Math.Ceiling(InternalChildren.Count/(double)columns)*RowHeight);
    }
    protected override Size ArrangeOverride(Size final)
    {
        var columns=Columns(final.Width);var width=final.Width/columns;
        for(var i=0;i<InternalChildren.Count;i++)InternalChildren[i].Arrange(new Rect(i%columns*width,i/columns*RowHeight,width,RowHeight));
        return final;
    }
}

public sealed class PhotoTreeStyleSelector : StyleSelector
{
    public Style? PhotoGroupStyle { get; set; }
    public override Style? SelectStyle(object item,DependencyObject container)=>item is PhotoGroup ? PhotoGroupStyle : null;
}
