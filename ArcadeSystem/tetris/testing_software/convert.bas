Attribute VB_Name = "Module1"
Sub convert()
    
    Dim row, col As Integer
    Dim clr As Long
    Dim str As String
    
    Open ThisWorkbook.Path & "\board_fmt.txt" For Output As #1
    
    Range("A1").Select
    For row = 0 To 15
        str = ""
        For col = 0 To 31
            Range("A1").Offset(row, col).Select
            clr = ActiveCell.Interior.Color
            For Each c In Range("A18", "I18").Cells
                If clr = c.Interior.Color Then
                    str = str & c.Value
                    Exit For
                End If
            Next
        Next
        Print #1, str
    Next
    
    
    Close #1
End Sub

